/**
 * @file user_config_parser.cpp
 * @brief User-config validation, indexing, settings loading, and parse orchestration.
 */

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <uconf_generator/config_transformer.hpp>
#include <uconf_parser/parser_internal.hpp>
#include <uconf_parser/user_config_parser.hpp>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <utils/yaml_utils.hpp>
#include <vector>

namespace {

    /**
     * @brief Resolve a named manifest path relative to the work directory.
     *
     * @param manifest        Parsed project manifest.
     * @param work_directory  Base Kez work directory.
     * @param name            Key below @c manifest.paths.
     * @return Resolved filesystem path.
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

    std::string optional_scalar(const YAML::Node& node, const std::string& key) {
        if (!yaml_has(node, key) || node[key].IsNull()) {
            return {};
        }
        return yaml_scalar(node[key], "setting '" + key + "'");
    }

    void index_named_user_values(const YAML::Node& sequence, const std::string& description,
                                 std::unordered_map<std::string, YAML::Node>& result) {
        if (!sequence.IsDefined() || sequence.IsNull()) {
            return;
        }
        if (!sequence.IsSequence()) {
            user_config_error(description + " must be a sequence");
        }
        result.reserve(sequence.size());
        for (const YAML::Node& candidate : sequence) {
            if (!candidate.IsMap() || !yaml_has(candidate, "name")) {
                user_config_error(description + " entries must contain a name");
            }
            result.emplace(yaml_scalar(candidate["name"], description + " name"), candidate);
        }
    }

    UserConfigurationIndex index_user_configuration(const YAML::Node& configuration) {
        UserConfigurationIndex result;
        if (yaml_has(configuration, "environment")) {
            index_named_user_values(configuration["environment"], "user environment configuration",
                                    result.environment);
        }
        if (yaml_has(configuration, "options")) {
            index_named_user_values(configuration["options"], "user option configuration",
                                    result.options);
        }
        return result;
    }

    struct UserStageNodeIndex {
        std::optional<YAML::Node> untargeted_stage;
        std::unordered_map<std::string, YAML::Node> targeted_stages;
    };

    UserStageNodeIndex index_user_stages(const YAML::Node& user_package) {
        UserStageNodeIndex result;
        if (!yaml_has(user_package, "build") || !yaml_has(user_package["build"], "stages")) {
            return result;
        }
        const YAML::Node stages = user_package["build"]["stages"];
        if (!stages.IsSequence()) {
            user_config_error("build.stages must be a sequence");
        }
        result.targeted_stages.reserve(stages.size());
        for (const YAML::Node& candidate : stages) {
            if (!candidate.IsMap()) {
                user_config_error("build.stages entries must be maps");
            }
            const YAML::Node target =
                yaml_has(candidate, "target") ? candidate["target"] : YAML::Node();
            if (!target.IsDefined() || target.IsNull()) {
                if (!result.untargeted_stage.has_value()) {
                    result.untargeted_stage = candidate;
                }
            } else if (target.IsScalar()) {
                result.targeted_stages.emplace(target.Scalar(), candidate);
            }
        }
        return result;
    }

    YAML::Node indexed_user_stage(const UserStageNodeIndex& stages, const BuildStage& stage) {
        if (!stage.target.has_value()) {
            return stages.untargeted_stage.value_or(YAML::Node());
        }
        const auto user_stage = stages.targeted_stages.find(*stage.target);
        return user_stage == stages.targeted_stages.end() ? YAML::Node() : user_stage->second;
    }

    YAML::Node user_configuration(const YAML::Node& user_node) {
        return yaml_has(user_node, "configurations") ? user_node["configurations"] : YAML::Node();
    }

    void index_user_package_configurations(ParsedUserPackage& package) {
        if (!package.transformed_build.has_value()) {
            return;
        }

        const YAML::Node user_build =
            yaml_has(package.user_config, "build") ? package.user_config["build"] : YAML::Node();
        package.build_configuration_index =
            index_user_configuration(user_configuration(user_build));

        const Build& build              = *package.transformed_build;
        const UserStageNodeIndex stages = index_user_stages(package.user_config);
        package.stage_configuration_indices.reserve(build.stages.size());
        for (const BuildStage& stage : build.stages) {
            package.stage_configuration_indices.push_back(
                index_user_configuration(user_configuration(indexed_user_stage(stages, stage))));
        }
    }

    std::string database_version(const YAML::Node& user_package) {
        if (!yaml_has(user_package, "version")) {
            return "latest";
        }
        std::string version         = yaml_scalar(user_package["version"], "package version");
        const std::size_t separator = version.find('@');
        return separator == std::string::npos ? version : version.substr(0, separator);
    }

    std::string package_compiler(const YAML::Node& user_package) {
        return yaml_has(user_package, "compiler")
                   ? yaml_scalar(user_package["compiler"], "package compiler")
                   : "system";
    }

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
                                      "' does not implement abstract package '" + abstract_name +
                                      "'");
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
            std::optional<Build> transformed_build = uconf_generator::transformed_build(
                *config, context.dependencies, context.abstract_packages,
                package_compiler(user_package));
            const std::size_t index = context.packages.size();
            context.package_indices.emplace(dependency, index);
            context.package_aliases.emplace(config->name, dependency);
            ParsedUserPackage package {
                dependency, user_package, std::move(config), std::move(transformed_build), {}, {}};
            index_user_package_configurations(package);
            context.packages.push_back(std::move(package));
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
    precompute_parser_values(context);

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
    return parse_user_config(load_yaml_file(path), settings);
}
