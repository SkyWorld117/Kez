#include <algorithm>
#include <user_config_generator/requirements_filter.hpp>

bool requirements_satisfied(const std::vector<std::string>& requirements,
                            const std::vector<std::string>& all_dependencies,
                            const AbstractPackageSelections& abstract_packages) {
    for (const std::string& requirement : requirements) {
        if (std::find(all_dependencies.begin(), all_dependencies.end(), requirement) !=
            all_dependencies.end()) {
            continue;
        }

        const auto selected = abstract_packages.find(requirement);
        if (selected != abstract_packages.end() &&
            std::find(all_dependencies.begin(), all_dependencies.end(), selected->second) !=
                all_dependencies.end()) {
            continue;
        }
        return false;
    }
    return true;
}
