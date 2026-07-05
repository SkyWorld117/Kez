#include <algorithm>
#include <dependency_resolver/requirements.hpp>

namespace {
    template <typename Dependencies, typename Contains>
    bool requirements_satisfied_impl(const std::vector<std::string>& requirements,
                                     const Dependencies& dependencies,
                                     const AbstractPackageSelections& abstract_packages,
                                     Contains contains) {
        for (const std::string& requirement : requirements) {
            if (contains(dependencies, requirement)) {
                continue;
            }
            const auto selected = abstract_packages.find(requirement);
            if (selected == abstract_packages.end() || !contains(dependencies, selected->second)) {
                return false;
            }
        }
        return true;
    }
}  // namespace

bool requirements_satisfied(const std::vector<std::string>& requirements,
                            const std::vector<std::string>& dependencies,
                            const AbstractPackageSelections& abstract_packages) {
    return requirements_satisfied_impl(
        requirements, dependencies, abstract_packages,
        [](const std::vector<std::string>& values, const std::string& value) {
            return std::find(values.begin(), values.end(), value) != values.end();
        });
}

bool requirements_satisfied(const std::vector<std::string>& requirements,
                            const std::unordered_set<std::string>& dependencies,
                            const AbstractPackageSelections& abstract_packages) {
    return requirements_satisfied_impl(
        requirements, dependencies, abstract_packages,
        [](const std::unordered_set<std::string>& values, const std::string& value) {
            return values.find(value) != values.end();
        });
}
