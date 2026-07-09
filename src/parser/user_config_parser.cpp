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
     * @brief Read an optional scalar string from a YAML map, returning an
     *        empty string when the key is missing or its value is null.
     *
     * Unlike yaml_scalar(), this function does not require the value to be
     * present -- it gracefully returns an empty string for missing or null
     * entries.  Only calls user_config_error if the key exists and its value
     * is not a scalar (e.g. a map or sequence).
     *
     * @param node  The YAML map node to search.
     * @param key   The map key whose string value is desired.
     * @return The scalar string value, or an empty string if the key is
     *         absent or null.
     */
    std::string optional_scalar(const YAML::Node& node, const std::string& key) {
        if (!yaml_has(node, key) || node[key].IsNull()) {
            return {};
        }
        return yaml_scalar(node[key], "setting '" + key + "'");
    }

    /**
     * @brief Search a YAML sequence of named maps for a specific entry.
     *
     * Iterates over the sequence and returns the first map whose `"name"`
     * field matches @p name.  Each element in the sequence is expected to be
     * a map containing a `"name"` scalar; any element that violates this is
     * treated as a fatal error.
     *
     * @param sequence    The YAML sequence to search.  May be undefined or
     *                    null, in which case an undefined node is returned.
     * @param name        The name value to match against each entry's
     *                    `"name"` field.
     * @param description Human-readable label for the sequence, used in
     *                    error messages (e.g. `"user option configuration"`).
     * @return The matching YAML map node, or an undefined node if no match
     *         is found.
     *
     * @warning Terminates via user_config_error() if @p sequence is not a
     *          sequence, or if any element lacks a `"name"` field or is not
     *          a map.
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
     * @brief Check whether a database target specification matches a user's
     *        target specification.
     *
     * A match occurs when:
     *   - Both are absent (database target is absent, and user target is
     *     undefined or null), OR
     *   - Both are present, the user target is a scalar, and its string
     *     value equals the database target string.
     *
     * @param database_target  Optional target from the database recipe
     *                         (e.g. a stage target name).  May be disengaged.
     * @param user_target      YAML node from the user configuration
     *                         (possibly undefined).
     * @return true if the two specifications are semantically equal.
     */
    bool targets_match(const std::optional<std::string>& database_target,
                       const YAML::Node& user_target) {
        if (!database_target.has_value()) {
            return !user_target.IsDefined() || user_target.IsNull();
        }
        return user_target.IsScalar() && user_target.Scalar() == *database_target;
    }

    /**
     * @brief Format an option name according to the build toolchain's
     *        conventions.
     *
     * Maps a bare option name to the flag syntax expected by the toolchain:
     *   - Autotools: prepends `--` (or returns the name as-is if it already
     *     starts with `--`, `-`, or is a shell assignment like `VAR=value`).
     *   - CMake: prepends `-D` (or returns as-is if already starting with `-`).
     *   - Other toolchains (Make, None): returns the name verbatim.
     *
     * @param name      The raw option name (e.g. `"shared"`, `"BUILD_SHARED_LIBS"`).
     * @param toolchain The build system toolchain in use.
     * @return The toolchain-formatted option string suitable for inclusion in
     *         a configuration command.
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
     * @brief Render a single build option into its command-line flag string.
     *
     * Uses the option's enabled/disabled format template and the corresponding
     * resolved value from @p state to produce the final flag.  If the format
     * resolves to an empty string the option contributes nothing to the command.
     *
     * @param option    The build option descriptor from the database recipe.
     * @param state     The parsed state indicating whether the option is
     *                  enabled and what value strings to use.
     * @param toolchain The build toolchain, used to format the flag name.
     * @param context   Parser context for resolving template placeholders
     *                  in the option values.
     * @return The rendered command-line fragment (may be empty).
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
     *        space-separated command-line string.
     *
     * Iterates over every BuildOption in @p configuration, looks up its
     * resolved state in @p context.option_values, renders each into a flag
     * fragment, and joins non-empty fragments with spaces.
     *
     * @param configuration  The build configuration whose options are rendered.
     * @param toolchain      The build toolchain used to format each flag.
     * @param context        Parser context holding the resolved option states.
     * @return A space-joined string of rendered flags, or an empty string if
     *         no options contribute anything.
     *
     * @warning Terminates via user_config_error() if an option from the
     *          configuration has no corresponding entry in
     *          @p context.option_values.
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
     * @brief Locate a user's build-stage override that matches a given
     *        database stage.
     *
     * Searches the user's `build.stages` sequence for the first entry whose
     * `"target"` field matches @p stage.target via targets_match().
     *
     * @param user_package  The user's YAML configuration for this package.
     * @param stage         The database stage descriptor to match against.
     * @return The matching user stage YAML node, or an undefined node if no
     *         match is found.
     *
     * @warning Terminates via user_config_error() if `build.stages` is
     *          present but is not a sequence, or if any entry is not a map.
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
     * @brief Retrieve the user's configuration overrides for a package,
     *        optionally scoped to a specific build stage.
     *
     * When @p stage is null (the default), returns the top-level
     * `build.configurations` node from the user config.  When a stage is
     * given, the function first finds the matching user stage via
     * find_user_stage() and returns that stage's `configurations` node.
     *
     * @param user_package  The user's YAML configuration for this package.
     * @param stage         Optional pointer to a specific build stage whose
     *                      per-stage overrides should be retrieved.  Pass
     *                      nullptr for the top-level configuration.
     * @return The user's configuration YAML node (may be undefined if no
     *         overrides exist for the requested scope).
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
     * @brief Read a user-supplied string value for a configurable field,
     *        falling back to a database default.
     *
     * If the user has provided an explicit value under @p key in
     * @p user_value, it is returned (with a null value mapping to an empty
     * string).  Otherwise the database's `ConfigurableValue.default_value` is
     * used, or an empty string if even that is absent.
     *
     * @param user_value  The user's YAML node that may contain the key.
     * @param key         The key to look up in @p user_value (e.g.
     *                    `"enabled_value"`, `"disabled_value"`).
     * @param value       The database's ConfigurableValue descriptor,
     *                    providing the fallback default.
     * @return The resolved string value.
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
     *
     * Two states are equal when their @p enabled flag and both value strings
     * are identical.
     *
     * @param left   The first parsed option state.
     * @param right  The second parsed option state.
     * @return true if all three fields match.
     */
    bool option_state_equal(const ParsedOptionState& left, const ParsedOptionState& right) {
        return left.enabled == right.enabled && left.enabled_value == right.enabled_value &&
               left.disabled_value == right.disabled_value;
    }

    /**
     * @brief Compare two maps of parsed option states for equality.
     *
     * Performs a structural comparison: the maps must have the same set of
     * keys, and each corresponding ParsedOptionState must be equal.
     *
     * @param left   The first option-state map.
     * @param right  The second option-state map.
     * @return true if both maps are identical in keys and values.
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
     * @brief Compute the resolved values for every environment variable in a
     *        build configuration.
     *
     * For each EnvironmentVariable in @p configuration:
     *   1. If the variable's `requires` conditions are unsatisfied, it is
     *      skipped (value left empty / unset).
     *   2. If the variable is user-configurable and user values are requested
     *      (@p use_user_values == true), the value is read from the user's
     *      YAML configuration.  The user must have provided an entry; if not,
     *      the program terminates.
     *   3. Otherwise the database default value is used.
     *   4. When @p apply_conditions is true, conditional overrides from the
     *      database recipe are evaluated and applied to the value.
     *
     * Results are stored in both @p context.environment_values (keyed by
     * pointer identity) and @p context.named_environment_values (keyed by
     * the string `"<package>.env.<variable>"`).
     *
     * @param configuration    The build configuration whose environment is
     *                         computed.
     * @param user_configuration  The user's YAML overrides for this
     *                         configuration (may be undefined).
     * @param package          The database package descriptor, providing
     *                         the package name for error messages and naming.
     * @param context          Parser context; output states are stored here.
     * @param use_user_values  Whether to read values from the user's YAML
     *                         configuration instead of database defaults.
     * @param apply_conditions Whether to evaluate conditional overrides on
     *                         each variable's value.
     *
     * @warning Terminates via user_config_error() if a user-configurable
     *          variable is missing from the user's configuration when
     *          @p use_user_values is true.
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
     * @brief Compute the resolved enabled/disabled state and value strings
     *        for every build option in a configuration.
     *
     * For each BuildOption in @p configuration:
     *   1. If the option's `requires` conditions are unsatisfied, it is
     *      forced disabled and its values are left empty.
     *   2. If the option is user-configurable and @p use_user_values is true,
     *      the user's YAML is consulted for the enabled flag and value
     *      overrides.  A missing entry is a fatal error.
     *   3. Otherwise the database defaults are used (option enabled unless
     *      the database says otherwise).
     *   4. When @p apply_conditions is true, conditional overrides from the
     *      database recipe are evaluated and applied.
     *
     * Results are stored in @p context.option_values (keyed by pointer
     * identity) and also written to @p context.named_option_values (keyed by
     * `"<package>.config.<option>"`).
     *
     * @param configuration    The build configuration whose options are
     *                         computed.
     * @param user_configuration  The user's YAML overrides for this
     *                         configuration (may be undefined).
     * @param package          The database package descriptor, providing the
     *                         package name for error messages and naming.
     * @param context          Parser context; output states are stored here.
     * @param use_user_values  Whether to read state from the user's YAML
     *                         configuration instead of database defaults.
     * @param apply_conditions Whether to evaluate conditional overrides on
     *                         each option's enabled/value fields.
     *
     * @warning Terminates via user_config_error() if:
     *         - A user-configurable option is missing from the user's config
     *           when @p use_user_values is true.
     *         - A user-configurable option has neither an explicit `"enabled"`
     *           field nor a database default with conditions.
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
     * @brief Compute both environment variables and options for a single
     *        build configuration.
     *
     * Convenience wrapper that calls compute_environment() and
     * compute_options() in sequence for the same configuration.
     *
     * @param configuration    The build configuration to compute.
     * @param user_configuration  The user's YAML overrides.
     * @param package          The database package descriptor.
     * @param context          Parser context (stores results).
     * @param use_user_values  Whether to use user-supplied values.
     * @param apply_conditions Whether to evaluate conditional overrides.
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
     * @brief Compute (or recompute) option and environment values for every
     *        package in the context.
     *
     * Iterates over all resolved packages.  For each package that has a
     * transformed build, the top-level build configurations and every per-stage
     * configuration are processed via compute_configuration().  Compiler and
     * MPI packages are treated specially: user values are used only when the
     * user has explicitly provided a `"build"` section for them.
     *
     * After computation the function compares the new named option and
     * environment maps against the previous snapshots (passed via @p context)
     * to determine whether any values changed.
     *
     * @param context          Parser context; option/environment maps are
     *                         both read (for previous snapshots) and written.
     * @param apply_conditions Whether to evaluate conditional overrides
     *                         during this pass.
     * @return true if any option or environment value changed during this
     *         pass; false if the state is stable (identical to the snapshots
     *         taken at the start of the call).
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
     * @brief Precompute all configurable option and environment values,
     *        iterating until the conditional values converge.
     *
     * First runs a pass with @p apply_conditions set to false to establish
     * baseline values from user input and database defaults.  Then repeatedly
     * recomputes with conditional overrides enabled until the state stabilises
     * (no value changes between passes).  The maximum number of iterations is
     * bounded by the total number of option states plus environment states
     * plus one, which provides an upper bound on the number of sequential
     * condition-dependency chains.
     *
     * @param context  Parser context; option and environment maps are
     *                 populated and iteratively refined.
     *
     * @warning Terminates via user_config_error() if the iterative
     *          computation does not converge within the bounded number of
     *          passes, indicating a circular condition dependency.
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
     * @brief Generate shell commands for a build configuration and append
     *        them to the command list.
     *
     * Resolves the configuration command template against the current context,
     * renders all options into command-line flags, and assembles the final
     * command string.  Before emitting the command, environment variables
     * declared in the configuration are exported (saving and restoring any
     * pre-existing values around the command).
     *
     * @param configuration  The build configuration whose command and
     *                       environment are to be emitted.
     * @param package        The database package descriptor (unused except
     *                       for signature compatibility).
     * @param toolchain      The build toolchain used to format option flags.
     * @param command        The command template string (may be empty).
     * @param context        Parser context for resolving templates and
     *                       looking up environment variable values.
     * @param commands       Output vector of shell commands.  The environment
     *                       exports, the resolved command, and the environment
     *                       restores are appended in order.
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
     * @brief Generate shell commands to apply user-selected patches and
     *        append them to the command list.
     *
     * Reads the `"patches"` list from the user's package configuration.
     * Each entry must be a map with `"name"` and `"enabled"` fields.  Enabled
     * patches are applied via `git apply`.  The patch file must exist at
     * `$KEZ_HOME/patches/<package>/<name>`.
     *
     * @param package   The parsed user package whose `"patches"` section is
     *                  consulted.
     * @param context   Parser context providing filesystem paths (kez_home).
     * @param commands  Output vector; `git apply` commands are appended for
     *                  each enabled patch.
     *
     * @warning Terminates via user_config_error() if:
     *         - The `"patches"` field is present but is not a sequence.
     *         - Any patch entry is not a map or lacks `"name"` or `"enabled"`.
     *         - A patch name contains path separators.
     *         - The referenced patch file does not exist.
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
     * @brief Generate the complete install command sequence for a single
     *        package.
     *
     * Produces the ordered list of shell commands that will download,
     * configure, build, and install the package.  The command sequence is:
     *   1. (Vendor packages only) Create the install prefix directory if it
     *      does not already exist.
     *   2. Source commands (download / unpack via append_source_commands()).
     *   3. Patch commands (via append_patch_commands()).
     *   4. Preprocessing shell fragment (if declared).
     *   5. Configuration command with options (if build configurations exist).
     *   6. Per-stage commands (configure, build, install, etc.).
     *   7. Postprocessing shell fragment (if declared).
     *
     * Compiler and MPI packages with no explicit `"build"` section in the
     * user config are skipped (they produce no commands).  Vendor packages
     * whose prefix already exists are also skipped as already-installed.
     *
     * @param package   The resolved user package for which commands are
     *                  generated.
     * @param context   Parser context for resolving templates and looking up
     *                  option/environment states.
     * @return A vector of shell command strings to be executed in order.
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
     *        mappings.
     *
     * Given a dependency name from the database recipe:
     *   1. If the name maps through @p context.abstract_packages to a
     *      concrete implementation, the concrete name is used.
     *   2. If the concrete name then maps through @p context.package_aliases
     *      (user-specified aliases), the alias target is returned.
     *   3. Otherwise the original (or first-resolved) name is returned
     *      unchanged.
     *
     * @param dependency  The dependency name from the database recipe.
     * @param context     Parser context providing abstract-package and alias
     *                    mappings.
     * @return The final resolved dependency name for plan construction.
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
     * @brief Recursively collect the nearest buildable packages reachable
     *        from a given dependency.
     *
     * A package may depend on a property-only "facade" package (e.g.
     * `nvhpc-nccl`) that declares no source or build and therefore emits no
     * commands, making it absent from the executable plan.  Such a package
     * imposes no scheduling constraint itself, but the buildable packages
     * reachable through it do -- a dependent must still wait for them.
     *
     * This function traverses the dependency graph starting from
     * @p dependency, recursing only through non-buildable intermediates and
     * stopping at buildable ones (whose own plan edges already encode their
     * deeper dependencies).  The @p visited set bounds the traversal and
     * breaks any accidental cycles.
     *
     * @param dependency         The name of the dependency to start from
     *                           (as used in the database recipe).
     * @param context            Parser context for package indices and
     *                           alias/abstract resolution.
     * @param buildable_packages Set of package names that produce commands
     *                           in the generated plan.
     * @param visited            Set tracking already-traversed package names
     *                           to prevent cycles.
     * @param result             Output vector to which buildable package
     *                           names are appended (via append_unique()).
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
     * @brief Append buildable dependencies that satisfy a set of requirement
     *        expressions to the result vector.
     *
     * Checks whether all expressions in @p requirements are satisfied via
     * requirements_satisfied().  If so, each requirement is expanded through
     * collect_buildable_dependencies() to find the nearest buildable packages
     * reachable from it.
     *
     * @param requirements       List of requirement expressions (package
     *                           names or abstract package keys) to process.
     * @param context            Parser context for package resolution.
     * @param buildable_packages Set of packages that produce build commands.
     * @param visited            Set tracking already-traversed package names
     *                           (shared across calls to prevent cycles).
     * @param result             Output vector; buildable package names are
     *                           appended.
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
     * @brief Collect buildable dependency packages implied by a build
     *        configuration's environment variables and options.
     *
     * For each environment variable and build option in @p configuration,
     * the function evaluates their respective `requires` lists via
     * append_plan_requirements() and appends any resulting buildable
     * packages to @p result.
     *
     * @param configuration      The build configuration whose option and
     *                           environment requirements are scanned.
     * @param context            Parser context for resolution.
     * @param buildable_packages Set of packages that produce build commands.
     * @param visited            Set tracking already-traversed packages
     *                           (shared across calls).
     * @param result             Output vector receiving buildable package
     *                           names.
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
     * @brief Generate the dependency list for a package in the install plan.
     *
     * Produces the names of buildable packages that must be installed before
     * @p package.  The algorithm proceeds in two phases:
     *   1. Direct dependencies from the database recipe are expanded through
     *      facades via collect_buildable_dependencies().
     *   2. If the package has a transformed build, additional requirements
     *      from configuration options and environment variables are appended.
     *
     * @param package            The parsed user package whose dependencies
     *                           are computed.
     * @param context            Parser context for resolution.
     * @param buildable_packages Set of packages that produce build commands
     *                           (i.e. are present in the generated plan).
     * @return A vector of buildable package names that must be installed
     *         before this package.
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
     * @brief Extract the version string from a user package configuration.
     *
     * Reads the `"version"` field from the user's YAML.  If the field is
     * absent, `"latest"` is returned.  If the version contains an `@`
     * separator (e.g. `"1.2.3@foo"`), everything after the `@` is stripped.
     *
     * @param user_package  The user's YAML configuration for the package.
     * @return The version string to use when looking up the database recipe.
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
     * @brief Extract the compiler name from a user package configuration.
     *
     * Reads the `"compiler"` field from the user's YAML.  If absent,
     * `"system"` is returned as the default compiler.
     *
     * @param user_package  The user's YAML configuration for the package.
     * @return The compiler name string (e.g. `"gcc"`, `"llvm"`, `"system"`).
     */
    std::string package_compiler(const YAML::Node& user_package) {
        return yaml_has(user_package, "compiler")
                   ? yaml_scalar(user_package["compiler"], "package compiler")
                   : "system";
    }

    /**
     * @brief Populate the parser context from the user configuration YAML.
     *
     * Performs the initial loading and validation phase of the parsing
     * pipeline:
     *   1. Validates the root structure (requires `"kez"` map and
     *      `"recipe.dependencies"` sequence).
     *   2. Loads the flat dependency set into @p context.dependencies.
     *   3. Processes abstract package selections from
     *      `"recipe.abstract_packages"` and validates that each
     *      implementation actually satisfies the abstract package.
     *   4. Verifies every key in the `"kez"` map corresponds to a declared
     *      dependency.
     *   5. For each dependency that is not system-provided, reads its user
     *      config, loads the database recipe, applies build transformation,
     *      and stores the result as a ParsedUserPackage in @p context.
     *
     * @param user_config  The top-level user configuration YAML node.
     * @param settings     Parser settings (paths, architecture, etc.).
     * @param context      Output context to populate.  Existing contents
     *                     are overwritten.
     *
     * @warning Terminates via user_config_error() on any structural or
     *          semantic validation failure (missing required fields, package
     *          absent from dependencies, abstract package mismatch, etc.).
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

BashCommandPlan parse_user_config(const YAML::Node& user_config,
                                  const std::filesystem::path& install_prefix) {
    return parse_user_config(user_config, load_user_config_parser_settings(install_prefix));
}

BashCommandPlan parse_user_config_file(const std::filesystem::path& path,
                                       const UserConfigParserSettings& settings) {
    if (!std::filesystem::is_regular_file(path)) {
        user_config_error("user configuration file does not exist: " + path.string());
    }
    return parse_user_config(YAML::LoadFile(path.string()), settings);
}
