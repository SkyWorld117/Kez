#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cstdlib>
#include <database/config.hpp>
#include <database/database.hpp>
#include <dependency_resolver/resolve_dependencies.hpp>
#include <filesystem>
#include <optional>
#include <string>
#include <uconf_generator/config_transformer.hpp>
#include <uconf_generator/configurations_filter.hpp>
#include <uconf_generator/stages_filter.hpp>
#include <uconf_generator/uconf_generator.hpp>
#include <ui/ui_utils.hpp>
#include <unordered_set>
#include <utils/bash_utils.hpp>
#include <utils/colored_io.hpp>
#include <utils/string_utils.hpp>
#include <utils/yaml_utils.hpp>
#include <vector>

namespace {

    /**
     * @brief Determine the default compiler family from the environment
     *        configuration.
     *
     * Reads the `KEZ_WORKDIR` environment variable and looks for a
     * `config.yaml` file inside that directory.
     *
     * @return The configured default compiler family as a string, or
     *         "system" if none is configured.
     */
    std::string configured_default_compiler() {
        const std::string work_directory = get_env_var_noerr("KEZ_WORKDIR");
        const std::filesystem::path path = std::filesystem::path(work_directory) / "config.yaml";
        const YAML::Node document        = cached_yaml_load(path);
        return yaml_scalar(document["settings"]["default_compiler"], "settings.default_compiler");
    }

    std::unordered_set<std::string> resolved_targets(
        const std::vector<std::string>& target_packages,
        const AbstractPackageSelections& abstract_packages) {
        std::unordered_set<std::string> result;
        result.reserve(target_packages.size());
        for (const std::string& target : target_packages) {
            const auto selected = abstract_packages.find(target);
            result.insert(selected == abstract_packages.end() ? target : selected->second);
        }
        return result;
    }

    std::vector<std::string> available_patches(const std::string& package_name) {
        const std::string home = get_env_var_noerr("KEZ_HOME");
        if (home.empty()) {
            return {};
        }

        const std::filesystem::path directory =
            std::filesystem::path(home) / "patches" / package_name;
        if (!std::filesystem::is_directory(directory)) {
            return {};
        }

        std::vector<std::string> result;
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                result.push_back(entry.path().filename().string());
            }
        }
        std::sort(result.begin(), result.end());
        return result;
    }

    std::string get_latest_existing_version(const std::string& package_name,
                                            const std::string& path_name, PackageType type) {
        const std::filesystem::path root = configured_work_path(path_name);
        if (!std::filesystem::is_directory(root)) {
            return {};
        }

        std::string latest_version;
        const std::string prefix = package_name + "-";

        for (auto it : std::filesystem::directory_iterator(root)) {
            if (!it.is_directory()) continue;
            std::string dirname = it.path().filename().string();
            if (dirname.rfind(prefix, 0) != 0) continue;
            std::string version = dirname.substr(prefix.size());
            if (type == PackageType::Mpi) {
                // Directory format: <name>-<version>-<compiler> where <compiler>
                // is either "system" or a "<compiler_name>-<compiler_version>"
                // pair.  MPI versions are semver-like (dots only, never dashes),
                // so the first dash always separates version from compiler.
                std::size_t first_dash = version.find('-');
                if (first_dash != std::string::npos) {
                    version = version.substr(0, first_dash);
                }
            }

            if (!version.empty()) {
                if (latest_version.empty() || compare_versions(version, latest_version) > 0) {
                    latest_version = version;
                }
            }
        }
        return latest_version;
    }

    void apply_option_list_selections(YAML::Node options,
                                      const std::unordered_map<std::string, bool>& selections) {
        if (!options || !options.IsSequence()) {
            return;
        }
        for (YAML::Node option : options) {
            if (!option["name"] || !option["name"].IsScalar()) {
                continue;
            }
            const auto selected = selections.find(option["name"].as<std::string>());
            if (selected != selections.end()) {
                option["enabled"] = selected->second;
            }
        }
    }

    /** @brief Apply interactive enabled states to top-level and stage options. */
    void apply_option_selections(YAML::Node package_output,
                                 const std::unordered_map<std::string, bool>& selections) {
        const YAML::Node build = package_output["build"];
        if (!build || !build.IsMap()) {
            return;
        }
        const YAML::Node configurations = build["configurations"];
        if (configurations && configurations.IsMap()) {
            apply_option_list_selections(configurations["options"], selections);
        }
        const YAML::Node stages = build["stages"];
        if (!stages || !stages.IsSequence()) {
            return;
        }
        for (YAML::Node stage : stages) {
            const YAML::Node stage_configurations = stage["configurations"];
            if (stage_configurations && stage_configurations.IsMap()) {
                apply_option_list_selections(stage_configurations["options"], selections);
            }
        }
    }

    void append_package_config(YAML::Node& output, const PackageConfig& package,
                               const std::vector<std::string>& all_dependencies,
                               const std::unordered_set<std::string>& all_dependency_set,
                               const std::unordered_set<std::string>& target_packages,
                               const AbstractPackageSelections& abstract_packages,
                               const InteractiveOptionSelections& option_selections,
                               const std::string& default_compiler) {
        YAML::Node package_output(YAML::NodeType::Map);
        if (package.description.has_value()) {
            package_output["description"] = *package.description;
        }
        if (package.source.has_value() && !package.source->releases.empty()) {
            std::string version = package.source->releases.front().version;
            if (package.type == PackageType::Mpi) {
                std::string existing =
                    get_latest_existing_version(package.name, "mpis", PackageType::Mpi);
                if (!existing.empty()) version = existing;
            } else if (package.type == PackageType::Vendor) {
                std::string existing =
                    get_latest_existing_version(package.name, "vendors", PackageType::Vendor);
                if (!existing.empty()) version = existing;
            }
            package_output["version"] = version;
        }
        if (package.type != PackageType::Vendor && package.type != PackageType::External) {
            package_output["compiler"] = default_compiler;
        }

        const std::vector<std::string> patches = available_patches(package.name);
        if (!patches.empty()) {
            YAML::Node patch_output(YAML::NodeType::Sequence);
            for (const std::string& patch : patches) {
                YAML::Node item(YAML::NodeType::Map);
                item["name"]    = patch;
                item["enabled"] = false;
                patch_output.push_back(item);
            }
            package_output["patches"] = patch_output;
        }

        const bool include_build =
            (package.type != PackageType::Compiler && package.type != PackageType::Mpi) ||
            target_packages.find(package.name) != target_packages.end();
        if (include_build && package.build.has_value()) {
            const std::optional<Build> build = uconf_generator::transformed_build(
                package, all_dependency_set, abstract_packages, default_compiler);
            YAML::Node build_output(YAML::NodeType::Map);
            if (build->configurations.has_value()) {
                YAML::Node configurations = filtered_configurations(
                    *build->configurations, all_dependencies, abstract_packages);
                if (configurations.size() != 0) {
                    build_output["configurations"] = configurations;
                }
            }

            YAML::Node stages = filtered_stages(build->stages, all_dependencies, abstract_packages);
            if (stages.size() != 0) {
                build_output["stages"] = stages;
            }
            if (build_output.size() != 0) {
                package_output["build"] = build_output;
            }
        }

        const auto selections = option_selections.find(package.name);
        if (selections != option_selections.end()) {
            apply_option_selections(package_output, selections->second);
        }

        output["kez"][package.name] = package_output;
    }
}  // namespace

YAML::Node gen_user_config(const std::vector<std::string>& package_names, bool interactive) {
    return gen_user_config(package_names, interactive, configured_default_compiler());
}

YAML::Node gen_user_config(const std::vector<std::string>& package_names, bool interactive,
                           const std::string& default_compiler) {
    InteractiveOptionSelections option_selections;
    DependencyResolution resolution =
        resolve_dependencies(package_names, interactive, &option_selections);
    const std::vector<std::string>& all_dependencies   = resolution.all_packages;
    const std::vector<std::string>& dependencies       = resolution.buildable_packages;
    const AbstractPackageSelections& abstract_packages = resolution.abstract_packages;

    if (dependencies.empty()) {
        std::string packages;
        for (const std::string& package : package_names) {
            packages += (packages.empty() ? "" : " ") + package;
        }
        ERROR("No dependencies found for packages: " + packages);
        exit(EXIT_FAILURE);
    }

    YAML::Node output(YAML::NodeType::Map);
    output["kez"]    = YAML::Node(YAML::NodeType::Map);
    output["recipe"] = YAML::Node(YAML::NodeType::Map);

    output["recipe"]["abstract_packages"] = YAML::Node(YAML::NodeType::Map);
    std::vector<std::string> abstract_names;
    abstract_names.reserve(abstract_packages.size());
    for (const auto& selection : abstract_packages) {
        abstract_names.push_back(selection.first);
    }
    std::sort(abstract_names.begin(), abstract_names.end());
    for (const std::string& abstract_package : abstract_names) {
        output["recipe"]["abstract_packages"][abstract_package] =
            abstract_packages.at(abstract_package);
    }

    output["recipe"]["dependencies"] = all_dependencies;
    output["recipe"]["targets"]      = package_names;

    const std::unordered_set<std::string> target_packages =
        resolved_targets(package_names, abstract_packages);
    const std::unordered_set<std::string> all_dependency_set(all_dependencies.begin(),
                                                             all_dependencies.end());
    for (const std::string& dependency : dependencies) {
        // Use the version that was resolved during dependency resolution.
        // If no version was explicitly resolved, "latest" is the default.
        const auto version_it = resolution.package_versions.find(dependency);
        const std::string ver =
            version_it == resolution.package_versions.end() ? "latest" : version_it->second;
        const PackageConfigPtr package = get_db_config(dependency, ver);
        append_package_config(output, *package, all_dependencies, all_dependency_set,
                              target_packages, abstract_packages, option_selections,
                              default_compiler);
    }
    return output;
}
