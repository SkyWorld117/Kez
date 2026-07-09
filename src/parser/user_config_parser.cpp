#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <database/config.hpp>
#include <dependency_resolver/requirements.hpp>
#include <filesystem>
#include <limits>
#include <optional>
#include <parser/parser_internal.hpp>
#include <parser/user_config_parser.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <user_config_generator/config_transformer.hpp>
#include <utility>
#include <utils/bash_utils.hpp>
#include <utils/string_utils.hpp>
#include <utils/yaml_utils.hpp>
#include <vector>

namespace {

    /**
     * @brief Resolve a named path from the manifest file relative to the work
     *        directory.
     *
     * Looks up @p name under `manifest["paths"]` and joins the result with
     * @p work_directory to produce an absolute path.  Terminates the program
     * if the manifest lacks the required `paths.<name>` entry.
     *
     * @param manifest        Parsed YAML node of the manifest file
     *                        (e.g. `manifest.yaml`).
     * @param work_directory  Base working directory whose child paths are
     *                        resolved against.
     * @param name            Key inside `manifest["paths"]` to look up
     *                        (e.g. `"system"`, `"compilers"`).
     * @return The resolved absolute filesystem path.
     *
     * @warning Terminates via user_config_error() if `manifest["paths"]` or
     *          `manifest["paths"][name]` is missing.
     */
    std::filesystem::path configured_path(const YAML::Node& manifest,
                                          const std::filesystem::path& work_directory,
                                          const std::string& name) {
        if (!yaml_has(manifest, "paths") || !yaml_has(manifest["paths"], name)) {
            user_config_error("manifest is missing paths." + name);
        }
        return work_directory /
               yaml_scalar(manifest["paths"][name], "manifest path '" + name + "'");
    }

    /**
     * @brief Return a scalar value from a YAML node if present and non-null,
     *        otherwise return an empty string.
     */
    std::string optional_scalar(const YAML::Node& node, const std::string& key) {
        if (!yaml_has(node, key) || node[key].IsNull()) {
            return {};
        }
        return yaml_scalar(node[key], "setting '" + key + "'");
    }

    /**
     * @brief Look up a named entry in a YAML sequence of maps by its \"name\"
     *        field.
     */
    YAML::Node find_named_user_value(const YAML::Node& sequence, const std::string& name,
                                     const std::string& description) {
        if (!sequence.IsDefined() || sequence.IsNull()) {
            return YAML::Node();
        }
        if (!sequence.IsSequence()) {
            user_config_error(description + " must be a sequence");
        }
        for (const YAML::Node& candidate : sequence) {
            if (!candidate.IsMap() || !yaml_has(candidate, "name")) {
                user_config_error(description + " entries must contain a name");
            }
            if (yaml_scalar(candidate["name"], description + " name") == name) {
                return candidate;
            }
        }
        return YAML::Node();
    }

    /**
     * @brief Check whether a database target matches a user-specified target.
     */
    bool targets_match(const std::optional<std::string>& database_target,
                       const YAML::Node& user_target) {
        if (!database_target.has_value()) {
            return !user_target.IsDefined() || user_target.IsNull();
        }
        return user_target.IsScalar() && user_target.Scalar() == *database_target;
    }

    /**
     * @brief Format an option name with the toolchain-appropriate prefix
     *        (e.g., \"--\" for Autotools, \"-D\" for CMake).
     */
    std::string option_name(const std::string& name, Toolchain toolchain) {
        if (toolchain == Toolchain::Autotools) {
            if (name.rfind("--", 0) == 0 || name.rfind('-', 0) == 0 || is_shell_assignment(name)) {
                return name;
            }
            return "--" + name;
        }
        if (toolchain == Toolchain::CMake) {
            return name.rfind('-', 0) == 0 ? name : "-D" + name;
        }
        return name;
    }

    /**
     * @brief Render a single build option to its command-line string
     *        representation, including any configured value.
     */
    std::string render_option(const BuildOption& option, const ParsedOptionState& state,
                              Toolchain toolchain, UserConfigParserContext& context) {
        const std::string format = state.enabled ? option.enabled_format.value_or(option.name)
                                                 : option.disabled_format.value_or("");
        if (format.empty()) {
            return {};
        }

        std::string result      = option_name(format, toolchain);
        const std::string value = resolve_parser_scalar(
            state.enabled ? state.enabled_value : state.disabled_value, context);
        if (!value.empty()) {
            result += "=" + shell_double_quote(value);
        }
        return result;
    }

    /**
     * @brief Render all options of a build configuration into a single
     *        space-joined command string.
     */
    std::string render_configuration_options(const BuildConfiguration& configuration,
                                             Toolchain toolchain,
                                             UserConfigParserContext& context) {
        std::vector<std::string> options;
        for (const BuildOption& option : configuration.options) {
            const auto parsed = context.option_values.find(&option);
            if (parsed == context.option_values.end()) {
                user_config_error("internal option state is missing for '" + option.name + "'");
            }
            const std::string rendered = render_option(option, parsed->second, toolchain, context);
            if (!rendered.empty()) {
                options.push_back(rendered);
            }
        }
        return join(options);
    }

    /**
     * @brief Find the user-provided build stage entry that matches a given
     *        database stage by target.
     */
    YAML::Node find_user_stage(const YAML::Node& user_package, const BuildStage& stage) {
        if (!yaml_has(user_package, "build") || !yaml_has(user_package["build"], "stages")) {
            return YAML::Node();
        }
        const YAML::Node stages = user_package["build"]["stages"];
        if (!stages.IsSequence()) {
            user_config_error("build.stages must be a sequence");
        }
        for (const YAML::Node& candidate : stages) {
            if (!candidate.IsMap()) {
                user_config_error("build.stages entries must be maps");
            }
            const YAML::Node target =
                yaml_has(candidate, "target") ? candidate["target"] : YAML::Node();
            if (targets_match(stage.target, target)) {
                return candidate;
            }
        }
        return YAML::Node();
    }

    /**
     * @brief Retrieve the user's configurations YAML node for a package,
     *        optionally scoped to a specific build stage.
     */
    YAML::Node user_configuration(const YAML::Node& user_package,
                                  const BuildStage* stage = nullptr) {
        if (stage == nullptr) {
            if (yaml_has(user_package, "build") &&
                yaml_has(user_package["build"], "configurations")) {
                return user_package["build"]["configurations"];
            }
            return YAML::Node();
        }
        const YAML::Node user_stage = find_user_stage(user_package, *stage);
        return yaml_has(user_stage, "configurations") ? user_stage["configurations"] : YAML::Node();
    }

    /**
     * @brief Resolve a configurable string value from user-provided YAML or
     *        fall back to the default.
     */
    std::string configurable_user_string(
        const YAML::Node& user_value, const std::string& key,
        const std::optional<ConfigurableValue<std::string>>& value) {
        if (user_value.IsDefined() && yaml_has(user_value, key)) {
            return user_value[key].IsNull()
                       ? ""
                       : yaml_scalar(user_value[key], "option field '" + key + "'");
        }
        return value.has_value() ? value->default_value.value_or("") : "";
    }

    /**
     * @brief Compare two ParsedOptionState values for equality.
     */
    bool option_state_equal(const ParsedOptionState& left, const ParsedOptionState& right) {
        return left.enabled == right.enabled && left.enabled_value == right.enabled_value &&
               left.disabled_value == right.disabled_value;
    }

    /**
     * @brief Compare two maps of named option states for equality.
     */
    bool option_values_equal(const std::unordered_map<std::string, ParsedOptionState>& left,
                             const std::unordered_map<std::string, ParsedOptionState>& right) {
        if (left.size() != right.size()) {
            return false;
        }
        for (const auto& [name, value] : left) {
            const auto parsed = right.find(name);
            if (parsed == right.end() || !option_state_equal(value, parsed->second)) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Compute environment variable values for a build configuration
     *        from user input or defaults, applying conditions when requested.
     */
    void compute_environment(const BuildConfiguration& configuration,
                             const YAML::Node& user_configuration, const PackageConfig& package,
                             UserConfigParserContext& context, bool use_user_values,
                             bool apply_conditions) {
        const YAML::Node user_environment = yaml_has(user_configuration, "environment")
                                                ? user_configuration["environment"]
                                                : YAML::Node();
        for (const EnvironmentVariable& variable : configuration.environment) {
            const bool required = requirements_satisfied(variable.requires, context.dependencies,
                                                         context.abstract_packages);
            std::string value;
            if (required && variable.user_configurable && use_user_values) {
                const YAML::Node user_value = find_named_user_value(
                    user_environment, variable.name, "user environment configuration");
                if (!user_value.IsDefined()) {
                    user_config_error("package '" + package.name +
                                      "' is missing configurable "
                                      "environment variable '" +
                                      variable.name + "'");
                }
                value = yaml_has(user_value, "value") && !user_value["value"].IsNull()
                            ? yaml_scalar(user_value["value"], "environment variable value")
                            : "";
            } else if (required) {
                value = variable.value.default_value.value_or("");
            }
            if (required && apply_conditions) {
                value = apply_parser_conditions(variable.value, value, context);
            }
            context.environment_values[&variable]                                    = value;
            context.named_environment_values[package.name + ".env." + variable.name] = value;
        }
    }

    /**
     * @brief Compute build option enabled/disabled states and their values
     *        from user input or defaults, applying conditions when requested.
     */
    void compute_options(const BuildConfiguration& configuration,
                         const YAML::Node& user_configuration, const PackageConfig& package,
                         UserConfigParserContext& context, bool use_user_values,
                         bool apply_conditions) {
        const YAML::Node user_options =
            yaml_has(user_configuration, "options") ? user_configuration["options"] : YAML::Node();
        for (const BuildOption& option : configuration.options) {
            const bool required = requirements_satisfied(option.requires, context.dependencies,
                                                         context.abstract_packages);
            YAML::Node user_value;
            if (required && option.user_configurable && use_user_values) {
                user_value =
                    find_named_user_value(user_options, option.name, "user option configuration");
                if (!user_value.IsDefined()) {
                    user_config_error("package '" + package.name +
                                      "' is missing configurable option '" + option.name + "'");
                }
            }

            ParsedOptionState state;
            if (!required) {
                state.enabled = false;
            } else if (option.user_configurable && use_user_values) {
                if (yaml_has(user_value, "enabled")) {
                    state.enabled = yaml_boolean(user_value["enabled"], "option enabled value");
                } else if (option.enabled.has_value() && !option.enabled->conditions.empty()) {
                    state.enabled = option.enabled->default_value.value_or(false);
                } else {
                    user_config_error("package '" + package.name + "' option '" + option.name +
                                      "' is missing its enabled value");
                }
            } else {
                state.enabled = option.enabled.has_value()
                                    ? option.enabled->default_value.value_or(true)
                                    : true;
            }

            if (required && option.enabled.has_value() && apply_conditions) {
                state.enabled = apply_parser_conditions(*option.enabled, state.enabled, context);
            }
            const std::string key            = package.name + ".config." + option.name;
            context.named_option_values[key] = state;

            if (required && option.user_configurable && use_user_values) {
                state.enabled_value =
                    configurable_user_string(user_value, "enabled_value", option.enabled_value);
                state.disabled_value =
                    configurable_user_string(user_value, "disabled_value", option.disabled_value);
            } else if (required) {
                state.enabled_value  = option.enabled_value.has_value()
                                           ? option.enabled_value->default_value.value_or("")
                                           : "";
                state.disabled_value = option.disabled_value.has_value()
                                           ? option.disabled_value->default_value.value_or("")
                                           : "";
            }
            if (required && option.enabled_value.has_value() && apply_conditions) {
                state.enabled_value =
                    apply_parser_conditions(*option.enabled_value, state.enabled_value, context);
            }
            if (required && option.disabled_value.has_value() && apply_conditions) {
                state.disabled_value =
                    apply_parser_conditions(*option.disabled_value, state.disabled_value, context);
            }
            context.option_values[&option]   = state;
            context.named_option_values[key] = state;
        }
    }

    /**
     * @brief Compute both environment variables and options for a build
     *        configuration in a single pass.
     */
    void compute_configuration(const BuildConfiguration& configuration,
                               const YAML::Node& user_configuration, const PackageConfig& package,
                               UserConfigParserContext& context, bool use_user_values,
                               bool apply_conditions) {
        compute_environment(configuration, user_configuration, package, context, use_user_values,
                            apply_conditions);
        compute_options(configuration, user_configuration, package, context, use_user_values,
                        apply_conditions);
    }

    /**
     * @brief Compute configuration values (options and environment) for all
     *        packages, optionally applying conditional resolution, and return
     *        whether any value changed.
     */
    bool compute_values(UserConfigParserContext& context, bool apply_conditions) {
        const auto previous_options     = context.named_option_values;
        const auto previous_environment = context.named_environment_values;

        for (const ParsedUserPackage& package : context.packages) {
            context.current_package = package.requested_name;
            if (!package.transformed_build.has_value()) {
                continue;
            }
            const Build& build         = *package.transformed_build;
            const bool use_user_values = (package.database_config->type != PackageType::Compiler &&
                                          package.database_config->type != PackageType::Mpi) ||
                                         yaml_has(package.user_config, "build");
            if (build.configurations.has_value()) {
                compute_configuration(
                    *build.configurations, user_configuration(package.user_config),
                    *package.database_config, context, use_user_values, apply_conditions);
            }
            for (const BuildStage& stage : build.stages) {
                if (stage.configurations.has_value()) {
                    compute_configuration(
                        *stage.configurations, user_configuration(package.user_config, &stage),
                        *package.database_config, context, use_user_values, apply_conditions);
                }
            }
        }
        return !option_values_equal(previous_options, context.named_option_values) ||
               previous_environment != context.named_environment_values;
    }

    /**
     * @brief Precompute configuration values by iterating conditional
     *        resolution passes until convergence is reached.
     */
    void precompute_values(UserConfigParserContext& context) {
        compute_values(context, false);

        const std::size_t max_passes =
            context.named_option_values.size() + context.named_environment_values.size() + 1;
        for (std::size_t pass = 0; pass < max_passes; ++pass) {
            if (!compute_values(context, true)) {
                return;
            }
        }
        user_config_error("conditional configuration values did not converge");
    }

    /**
     * @brief Append shell commands that export configuration options and
     *        environment variables, execute the build command, and restore
     *        previous environment state.
     */
    void append_configuration_commands(const BuildConfiguration& configuration,
                                       const PackageConfig& package, Toolchain toolchain,
                                       const std::string& command, UserConfigParserContext& context,
                                       std::vector<std::string>& commands) {
        std::string resolved_command = resolve_parser_scalar(command, context);
        (void) package;
        const std::string options = render_configuration_options(configuration, toolchain, context);
        if (!options.empty()) {
            resolved_command += (resolved_command.empty() ? "" : " ") + options;
        }
        if (resolved_command.empty()) {
            return;
        }

        std::vector<std::pair<std::string, std::optional<std::string>>> previous_values;
        for (const EnvironmentVariable& variable : configuration.environment) {
            const auto parsed = context.environment_values.find(&variable);
            if (parsed == context.environment_values.end() || parsed->second.empty()) {
                continue;
            }
            const char* previous = std::getenv(variable.name.c_str());
            previous_values.emplace_back(variable.name, previous == nullptr
                                                            ? std::nullopt
                                                            : std::optional<std::string>(previous));
            commands.push_back("export " + variable.name + "=" +
                               shell_double_quote(resolve_parser_scalar(parsed->second, context)));
        }
        commands.push_back(resolved_command);
        for (const auto& [name, previous] : previous_values) {
            if (!previous.has_value()) {
                commands.push_back("unset " + name);
            } else {
                commands.push_back("export " + name + "=" + shell_single_quote(*previous));
            }
        }
    }

    /**
     * @brief Append git-apply commands for patches the user has enabled in
     *        their package configuration.
     */
    void append_patch_commands(const ParsedUserPackage& package, UserConfigParserContext& context,
                               std::vector<std::string>& commands) {
        if (!yaml_has(package.user_config, "patches")) {
            return;
        }
        const YAML::Node patches = package.user_config["patches"];
        if (!patches.IsSequence()) {
            user_config_error("package patches must be a sequence");
        }
        for (const YAML::Node& patch : patches) {
            if (!patch.IsMap() || !yaml_has(patch, "name") || !yaml_has(patch, "enabled")) {
                user_config_error("patch entries must contain name and enabled fields");
            }
            if (!yaml_boolean(patch["enabled"], "patch enabled value")) {
                continue;
            }
            const std::string name = yaml_scalar(patch["name"], "patch name");
            if (std::filesystem::path(name).filename() != name) {
                user_config_error("invalid patch name '" + name + "'");
            }
            const std::filesystem::path path =
                context.settings.kez_home / "patches" / package.requested_name / name;
            if (!std::filesystem::is_regular_file(path)) {
                user_config_error("patch file does not exist: " + path.string());
            }
            commands.push_back("git apply " + shell_single_quote(path.string()));
        }
    }

    /**
     * @brief Generate the full sequence of shell commands to build a single
     *        package: source extraction, patches, preprocessing,
     *        configuration, stages, and postprocessing.
     */
    std::vector<std::string> generate_package_commands(const ParsedUserPackage& package,
                                                       UserConfigParserContext& context) {
        std::vector<std::string> commands;
        context.current_package = package.requested_name;
        if (!package.transformed_build.has_value()) {
            return commands;
        }
        if ((package.database_config->type == PackageType::Compiler ||
             package.database_config->type == PackageType::Mpi) &&
            !yaml_has(package.user_config, "build")) {
            return commands;
        }
        if (package.database_config->type == PackageType::Vendor) {
            const std::filesystem::path prefix =
                parser_package_prefix(package.requested_name, context);
            if (std::filesystem::exists(prefix)) {
                return commands;
            }
            commands.push_back("mkdir -p " + shell_single_quote(prefix.string()));
        }

        append_source_commands(package, context, commands);
        append_patch_commands(package, context, commands);

        const Build& build = *package.transformed_build;
        if (build.preprocessing.has_value()) {
            commands.push_back(resolve_parser_scalar(*build.preprocessing, context));
        }
        if (build.configurations.has_value()) {
            const std::optional<std::string> default_command =
                package.database_config->default_configuration_command();
            append_configuration_commands(
                *build.configurations, *package.database_config,
                package.database_config->toolchain(),
                build.configurations->command.value_or(default_command.value_or("")), context,
                commands);
        }
        for (const BuildStage& stage : build.stages) {
            BuildStage resolved_stage = stage;
            if (resolved_stage.target.has_value()) {
                resolved_stage.target = resolve_parser_scalar(*resolved_stage.target, context);
            }
            const std::optional<std::string> default_command =
                package.database_config->default_stage_command(resolved_stage,
                                                               context.settings.parallel_jobs);
            const std::string command =
                stage.configurations.has_value() && stage.configurations->command.has_value()
                    ? *stage.configurations->command
                    : default_command.value_or("");
            if (stage.configurations.has_value()) {
                append_configuration_commands(*stage.configurations, *package.database_config,
                                              Toolchain::None, command, context, commands);
            } else if (!command.empty()) {
                commands.push_back(resolve_parser_scalar(command, context));
            }
        }
        if (build.postprocessing.has_value()) {
            commands.push_back(resolve_parser_scalar(*build.postprocessing, context));
        }
        return commands;
    }

    /**
     * @brief Resolve a dependency name through abstract-package and alias
     *        mappings to its final concrete name for the build plan.
     */
    std::string plan_dependency_name(const std::string& dependency,
                                     const UserConfigParserContext& context) {
        const auto selected = context.abstract_packages.find(dependency);
        const std::string resolved =
            selected == context.abstract_packages.end() ? dependency : selected->second;
        const auto alias = context.package_aliases.find(resolved);
        return alias == context.package_aliases.end() ? resolved : alias->second;
    }

    /**
     * @brief Recursively traverse the dependency tree to collect packages
     *        that have non-empty build commands.
     */
    void collect_buildable_dependencies(const std::string& dependency,
                                        const UserConfigParserContext& context,
                                        const std::unordered_set<std::string>& buildable_packages,
                                        std::unordered_set<std::string>& visited,
                                        std::vector<std::string>& result) {
        const std::string resolved = plan_dependency_name(dependency, context);
        if (!visited.insert(resolved).second) {
            return;
        }
        if (buildable_packages.find(resolved) != buildable_packages.end()) {
            append_unique(result, resolved);
            return;
        }
        const auto parsed = context.package_indices.find(resolved);
        if (parsed == context.package_indices.end()) {
            return;
        }
        for (const std::string& sub_dependency :
             context.packages[parsed->second].database_config->dependencies) {
            collect_buildable_dependencies(sub_dependency, context, buildable_packages, visited,
                                           result);
        }
    }

    /**
     * @brief Collect buildable dependencies for a set of requirements whose
     *        conditions are satisfied.
     */
    void append_plan_requirements(const std::vector<std::string>& requirements,
                                  const UserConfigParserContext& context,
                                  const std::unordered_set<std::string>& buildable_packages,
                                  std::unordered_set<std::string>& visited,
                                  std::vector<std::string>& result) {
        if (!requirements_satisfied(requirements, context.dependencies,
                                    context.abstract_packages)) {
            return;
        }
        for (const std::string& requirement : requirements) {
            collect_buildable_dependencies(requirement, context, buildable_packages, visited,
                                           result);
        }
    }

    /**
     * @brief Collect buildable dependencies referenced by configuration
     *        environment variables and options.
     */
    void append_plan_configuration_dependencies(
        const BuildConfiguration& configuration, const UserConfigParserContext& context,
        const std::unordered_set<std::string>& buildable_packages,
        std::unordered_set<std::string>& visited, std::vector<std::string>& result) {
        for (const EnvironmentVariable& variable : configuration.environment) {
            append_plan_requirements(variable.requires, context, buildable_packages, visited,
                                     result);
        }
        for (const BuildOption& option : configuration.options) {
            append_plan_requirements(option.requires, context, buildable_packages, visited, result);
        }
    }

    /**
     * @brief Generate the list of buildable dependencies required by a
     *        package, including those from its configuration.
     */
    std::vector<std::string> generate_package_dependencies(
        const ParsedUserPackage& package, const UserConfigParserContext& context,
        const std::unordered_set<std::string>& buildable_packages) {
        std::vector<std::string> result;
        std::unordered_set<std::string> visited;
        for (const std::string& dependency : package.database_config->dependencies) {
            collect_buildable_dependencies(dependency, context, buildable_packages, visited,
                                           result);
        }

        if (!package.transformed_build.has_value()) {
            return result;
        }
        const Build& build = *package.transformed_build;
        if (build.configurations.has_value()) {
            append_plan_configuration_dependencies(*build.configurations, context,
                                                   buildable_packages, visited, result);
        }
        for (const BuildStage& stage : build.stages) {
            if (stage.configurations.has_value()) {
                append_plan_configuration_dependencies(*stage.configurations, context,
                                                       buildable_packages, visited, result);
            }
        }
        return result;
    }

    /**
     * @brief Extract the database version string from a user package
     *        configuration, defaulting to \"latest\".
     */
    std::string database_version(const YAML::Node& user_package) {
        if (!yaml_has(user_package, "version")) {
            return "latest";
        }
        std::string version         = yaml_scalar(user_package["version"], "package version");
        const std::size_t separator = version.find('@');
        return separator == std::string::npos ? version : version.substr(0, separator);
    }

    /**
     * @brief Extract the compiler identifier from a user package
     *        configuration, defaulting to \"system\".
     */
    std::string package_compiler(const YAML::Node& user_package) {
        return yaml_has(user_package, "compiler")
                   ? yaml_scalar(user_package["compiler"], "package compiler")
                   : "system";
    }

    /**
     * @brief Populate a UserConfigParserContext from the user config YAML
     *        and settings: validate structure, load dependencies, resolve
     *        abstract packages, and parse each package entry.
     */
    void load_parser_context(const YAML::Node& user_config,
                             const UserConfigParserSettings& settings,
                             UserConfigParserContext& context) {
        if (!user_config.IsMap() || !yaml_has(user_config, "kez") || !user_config["kez"].IsMap()) {
            user_config_error("root must contain a kez map");
        }
        if (!yaml_has(user_config, "recipe") || !user_config["recipe"].IsMap() ||
            !yaml_has(user_config["recipe"], "dependencies") ||
            !user_config["recipe"]["dependencies"].IsSequence()) {
            user_config_error("recipe.dependencies must be a sequence");
        }

        context.user_config = user_config;
        context.settings    = settings;
        for (const YAML::Node& dependency : user_config["recipe"]["dependencies"]) {
            context.dependencies.insert(yaml_scalar(dependency, "dependency name"));
        }

        if (yaml_has(user_config["recipe"], "abstract_packages")) {
            const YAML::Node abstract_packages = user_config["recipe"]["abstract_packages"];
            if (!abstract_packages.IsMap()) {
                user_config_error("recipe.abstract_packages must be a map");
            }
            for (const auto& selection : abstract_packages) {
                const std::string abstract_name =
                    yaml_scalar(selection.first, "abstract package name");
                const std::string implementation =
                    yaml_scalar(selection.second, "abstract package implementation");
                const PackageConfigPtr config = get_db_config(abstract_name);
                if (config->type != PackageType::Abstract ||
                    std::find(config->implementations.begin(), config->implementations.end(),
                              implementation) == config->implementations.end()) {
                    user_config_error("'" + implementation +
                                      "' does not implement abstract "
                                      "package '" +
                                      abstract_name + "'");
                }
                context.abstract_packages.emplace(abstract_name, implementation);
                for (const std::string& candidate : config->implementations) {
                    ParsedOptionState state;
                    state.enabled = candidate == implementation;
                    context.named_option_values[abstract_name + ".use-" + candidate] = state;
                }
            }
        }

        const YAML::Node kez = user_config["kez"];
        for (const auto& entry : kez) {
            const std::string package_name = yaml_scalar(entry.first, "package name");
            if (context.dependencies.find(package_name) == context.dependencies.end()) {
                user_config_error("package '" + package_name +
                                  "' is absent from recipe.dependencies");
            }
        }

        for (const YAML::Node& dependency_node : user_config["recipe"]["dependencies"]) {
            const std::string dependency = yaml_scalar(dependency_node, "dependency name");
            if (context.package_indices.find(dependency) != context.package_indices.end()) {
                continue;
            }
            if (!yaml_has(kez, dependency)) {
                if (get_db_config(dependency)->type == PackageType::System) {
                    continue;
                }
                user_config_error("non-system dependency '" + dependency +
                                  "' is absent from the kez map");
            }
            const YAML::Node user_package = kez[dependency];
            if (!user_package.IsMap()) {
                user_config_error("package '" + dependency + "' must be a map");
            }
            PackageConfigPtr config = get_db_config(dependency, database_version(user_package));
            std::optional<Build> transformed_build = user_config_generator::transformed_build(
                *config, context.dependencies, context.abstract_packages,
                package_compiler(user_package));
            const std::size_t index = context.packages.size();
            context.package_indices.emplace(dependency, index);
            context.package_aliases.emplace(config->name, dependency);
            context.packages.push_back(
                {dependency, user_package, std::move(config), std::move(transformed_build)});
        }
    }

}  // namespace

/**
 * @brief Load parser settings from environment variables (KEZ_HOME,
 *        KEZ_WORKDIR, KEZ_ARCH), the manifest file, and the user config
 *        file under the work directory.
 */
UserConfigParserSettings load_user_config_parser_settings(
    const std::filesystem::path& install_prefix) {
    const char* home_value = std::getenv("KEZ_HOME");
    const char* work_value = std::getenv("KEZ_WORKDIR");
    const char* arch_value = std::getenv("KEZ_ARCH");
    if (home_value == nullptr || work_value == nullptr || arch_value == nullptr) {
        user_config_error("KEZ_HOME, KEZ_WORKDIR, and KEZ_ARCH must be set");
    }

    UserConfigParserSettings result;
    result.install_prefix = install_prefix;
    result.kez_home       = home_value;
    result.architecture   = arch_value;

    const std::filesystem::path work_directory = work_value;
    const YAML::Node manifest = cached_yaml_load(result.kez_home / "manifest.yaml");
    result.system_prefix      = configured_path(manifest, work_directory, "system");
    result.compilers_prefix   = configured_path(manifest, work_directory, "compilers");
    result.mpis_prefix        = configured_path(manifest, work_directory, "mpis");
    result.vendors_prefix     = configured_path(manifest, work_directory, "vendors");
    result.cache_prefix       = configured_path(manifest, work_directory, "cache");

    const YAML::Node config = cached_yaml_load(work_directory / "config.yaml");
    if (yaml_has(config, "settings")) {
        const YAML::Node settings = config["settings"];
        if (yaml_has(settings, "n_proc_for_build")) {
            const std::string jobs =
                yaml_scalar(settings["n_proc_for_build"], "settings.n_proc_for_build");
            std::size_t parsed        = 0;
            const unsigned long value = std::stoul(jobs, &parsed);
            if (parsed != jobs.size() || value == 0 ||
                value > std::numeric_limits<unsigned int>::max()) {
                user_config_error("settings.n_proc_for_build must be a positive integer");
            }
            result.parallel_jobs = static_cast<unsigned int>(value);
        }
        if (yaml_has(settings, "external")) {
            if (!settings["external"].IsMap()) {
                user_config_error("settings.external must be a map");
            }
            for (const auto& entry : settings["external"]) {
                const std::string name = yaml_scalar(entry.first, "external package name");
                if (!entry.second.IsMap()) {
                    user_config_error("external package setting '" + name + "' must be a map");
                }
                result.external_packages[name] = {optional_scalar(entry.second, "prefix"),
                                                  optional_scalar(entry.second, "version")};
            }
        }
    }

    const std::filesystem::path architecture_path =
        result.kez_home / "heuristics" / "architecture.yaml";
    if (std::filesystem::is_regular_file(architecture_path)) {
        const YAML::Node architecture = cached_yaml_load(architecture_path);
        if (yaml_has(architecture, "architecture")) {
            for (const auto& entry : architecture["architecture"]) {
                const std::string variant = yaml_scalar(entry.first, "architecture variant");
                if (entry.second.IsMap() && yaml_has(entry.second, result.architecture)) {
                    result.architecture_variants[variant] = yaml_scalar(
                        entry.second[result.architecture], "architecture variant value");
                }
            }
        }
    }
    return result;
}

/**
 * @brief Parse a user configuration YAML node into a BashCommandPlan using
 *        the provided settings: load context, precompute values, and
 *        generate build commands in dependency order.
 */
BashCommandPlan parse_user_config(const YAML::Node& user_config,
                                  const UserConfigParserSettings& settings) {
    UserConfigParserContext context;
    load_parser_context(user_config, settings, context);
    precompute_values(context);

    std::unordered_map<std::string, std::vector<std::string>> commands_by_package;
    for (const ParsedUserPackage& package : context.packages) {
        commands_by_package.emplace(package.requested_name,
                                    generate_package_commands(package, context));
    }
    std::unordered_set<std::string> buildable_packages;
    for (const auto& [package, commands] : commands_by_package) {
        if (!commands.empty()) {
            buildable_packages.insert(package);
        }
    }

    BashCommandPlan result;
    const YAML::Node dependencies = user_config["recipe"]["dependencies"];
    std::vector<std::string> dependency_names;
    dependency_names.reserve(dependencies.size());
    for (const YAML::Node& dependency : dependencies) {
        dependency_names.push_back(yaml_scalar(dependency, "dependency name"));
    }
    for (auto current = dependency_names.rbegin(); current != dependency_names.rend(); ++current) {
        const std::string& package = *current;
        const auto commands        = commands_by_package.find(package);
        if (commands != commands_by_package.end() && !commands->second.empty()) {
            const auto parsed = context.package_indices.find(package);
            if (parsed == context.package_indices.end()) {
                user_config_error("internal package state is missing for '" + package + "'");
            }
            result.push_back({package, commands->second,
                              generate_package_dependencies(context.packages[parsed->second],
                                                            context, buildable_packages)});
        }
    }
    return result;
}

/**
 * @brief Parse a user configuration YAML node into a BashCommandPlan,
 *        loading the settings from the given install prefix.
 */
BashCommandPlan parse_user_config(const YAML::Node& user_config,
                                  const std::filesystem::path& install_prefix) {
    return parse_user_config(user_config, load_user_config_parser_settings(install_prefix));
}

/**
 * @brief Read a user configuration file from disk and parse it into a
 *        BashCommandPlan.
 */
BashCommandPlan parse_user_config_file(const std::filesystem::path& path,
                                       const UserConfigParserSettings& settings) {
    if (!std::filesystem::is_regular_file(path)) {
        user_config_error("user configuration file does not exist: " + path.string());
    }
    return parse_user_config(YAML::LoadFile(path.string()), settings);
}
