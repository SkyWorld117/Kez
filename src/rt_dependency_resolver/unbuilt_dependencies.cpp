#include "unbuilt_dependencies.h"

std::vector<std::string> get_unbuilt_dependencies(const YAML::Node& user_config, const YAML::Node& ins_yaml, const std::vector<std::string>& installed_packages, const std::string& target_package) {
    std::vector<std::string> unbuilt_dependencies;

    // Find all the packages in the user configuration
    std::vector<std::string> user_packages;
    for (const auto& pkg_config : user_config["cheese"]) {
        user_packages.push_back(pkg_config.first.as<std::string>());
    }

    // Resolve all the dependencies for the target package
    std::vector<std::string> essential_dependencies = get_essential_dependencies(target_package);
    std::vector<std::string> optional_dependencies = get_optional_dependencies(target_package);
    // Combine essential and optional dependencies
    std::vector<std::string> all_dependencies;
    all_dependencies.insert(all_dependencies.end(), essential_dependencies.begin(), essential_dependencies.end());
    all_dependencies.insert(all_dependencies.end(), optional_dependencies.begin(), optional_dependencies.end());

    // Map abstract packages to real packages using user configuration
    std::unordered_map<std::string, std::string> abstract_to_real = user_config["recipe"]["abstract_packages"] ? 
        user_config["recipe"]["abstract_packages"].as<std::unordered_map<std::string, std::string>>() : 
        std::unordered_map<std::string, std::string>();
    for (auto& dep : all_dependencies) {
        if (abstract_to_real.find(dep) != abstract_to_real.end()) {
            dep = abstract_to_real[dep];
        }
    }

    // Find all the packages that build before the target package from the user configuration
    // The dependencies are strictly after the target package in the user configuration
    std::vector<std::string> preceding_packages;
    preceding_packages.insert(preceding_packages.end(), std::find(user_packages.begin(), user_packages.end(), target_package) + 1, user_packages.end());

    // Get the intersection of all_dependencies and preceding_packages (true dependencies because we include all optional dependencies)
    std::vector<std::string> true_dependencies;
    std::sort(all_dependencies.begin(), all_dependencies.end());
    std::sort(preceding_packages.begin(), preceding_packages.end());
    std::set_intersection(all_dependencies.begin(), all_dependencies.end(),
                          preceding_packages.begin(), preceding_packages.end(),
                          std::back_inserter(true_dependencies));

    // Find all unbuilt dependencies among the true dependencies
    for (const auto& dep : true_dependencies) {
        if (std::find(installed_packages.begin(), installed_packages.end(), dep) == installed_packages.end()) {
            unbuilt_dependencies.push_back(dep);
        }
    }

    return unbuilt_dependencies;
}