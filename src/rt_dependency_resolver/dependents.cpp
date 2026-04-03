#include <rt_dependency_resolver/dependents.hpp>

std::vector<std::string> get_dependents(const YAML::Node& user_config, const YAML::Node& ins_yaml,
                                        const std::vector<std::string>& installed_packages,
                                        const std::string& target_package) {
    std::vector<std::string> dependents;

    // Find all the packages in the user configuration
    std::vector<std::string> user_packages;
    for (const auto& pkg_config : user_config["cheese"]) {
        user_packages.push_back(pkg_config.first.as<std::string>());
    }

    // Iterate through all user packages to find those that depend on the target package
    for (const auto& pkg : user_packages) {
        if (pkg == target_package) {
            continue;  // Skip the target package itself
        }

        // Resolve all the dependencies for the current package
        std::vector<std::string> essential_dependencies = get_essential_dependencies(pkg);
        std::vector<std::string> optional_dependencies  = get_optional_dependencies(pkg);
        // Combine essential and optional dependencies
        std::vector<std::string> all_dependencies;
        all_dependencies.insert(all_dependencies.end(), essential_dependencies.begin(),
                                essential_dependencies.end());
        all_dependencies.insert(all_dependencies.end(), optional_dependencies.begin(),
                                optional_dependencies.end());

        // Check if the target package is among the dependencies
        if (std::find(all_dependencies.begin(), all_dependencies.end(), target_package) !=
            all_dependencies.end()) {
            dependents.push_back(pkg);
        }
    }

    return dependents;
}