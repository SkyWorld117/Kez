#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cstdlib>
#include <database/config.hpp>
#include <database/database.hpp>
#include <dependency_resolver/resolve_dependencies.hpp>
#include <filesystem>
#include <string>
#include <ui/ui_utils.hpp>
#include <unordered_set>
#include <user_config_generator/configurations_filter.hpp>
#include <user_config_generator/stages_filter.hpp>
#include <user_config_generator/user_config_generator.hpp>
#include <utils/bash_utils.hpp>
#include <utils/colored_io.hpp>
#include <utils/string_utils.hpp>
#include <utils/yaml_utils.hpp>
#include <vector>

namespace {
    std::string configured_default_compiler() {
        const std::string work_directory = get_env_var_noerr("KEZ_WORKDIR");
        if (work_directory.empty()) {
            return "system";
        }

        const std::filesystem::path path = std::filesystem::path(work_directory) / "config.yaml";
        if (!std::filesystem::is_regular_file(path)) {
            return "system";
        }

        const YAML::Node document = YAML::LoadFile(path.string());
        if (!yaml_has(document, "settings")) {
            return "system";
        }
        const YAML::Node settings = document["settings"];
        if (!settings.IsMap() || !yaml_has(settings, "default_compiler")) {
            return "system";
        }
        const YAML::Node compiler = settings["default_compiler"];
        if (!compiler.IsScalar()) {
            return "system";
        }
        return yaml_scalar(compiler, "settings.default_compiler");
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
                std::size_t last_dash = version.find_last_of('-');
                if (last_dash != std::string::npos) {
                    version = version.substr(0, last_dash);
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

    void append_package_config(YAML::Node& output, const PackageConfig& package,
                               const std::vector<std::string>& all_dependencies,
                               const std::unordered_set<std::string>& target_packages,
                               const AbstractPackageSelections& abstract_packages,
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
        if (package.build.has_value() && include_build) {
            YAML::Node build_output(YAML::NodeType::Map);
            if (package.build->configurations.has_value()) {
                YAML::Node configurations = filtered_configurations(
                    *package.build->configurations, all_dependencies, abstract_packages);
                if (configurations.size() != 0) {
                    build_output["configurations"] = configurations;
                }
            }

            YAML::Node stages =
                filtered_stages(package.build->stages, all_dependencies, abstract_packages);
            if (stages.size() != 0) {
                build_output["stages"] = stages;
            }
            if (build_output.size() != 0) {
                package_output["build"] = build_output;
            }
        }

        output["cheese"][package.name] = package_output;
    }
}  // namespace

YAML::Node gen_user_config(const std::vector<std::string>& package_names, bool interactive) {
    return gen_user_config(package_names, interactive, configured_default_compiler());
}

YAML::Node gen_user_config(const std::vector<std::string>& package_names, bool interactive,
                           const std::string& default_compiler) {
    DependencyResolution resolution = resolve_dependencies(package_names, interactive);
    const std::vector<std::string>& all_dependencies   = resolution.first.first;
    const std::vector<std::string>& dependencies       = resolution.first.second;
    const AbstractPackageSelections& abstract_packages = resolution.second;

    if (dependencies.empty()) {
        std::string packages;
        for (const std::string& package : package_names) {
            packages += (packages.empty() ? "" : " ") + package;
        }
        ERROR("No dependencies found for packages: " + packages);
        exit(EXIT_FAILURE);
    }

    YAML::Node output(YAML::NodeType::Map);
    output["cheese"] = YAML::Node(YAML::NodeType::Map);
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
    for (const std::string& dependency : dependencies) {
        const PackageConfigPtr package = get_db_config(dependency);
        append_package_config(output, *package, all_dependencies, target_packages,
                              abstract_packages, default_compiler);
    }
    return output;
}
