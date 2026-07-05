#include <database/config.hpp>
#include <database/database.hpp>
#include <dependency_resolver/optional_dependencies.hpp>
#include <unordered_set>

namespace {
    void append_requirements(const std::vector<std::string>& requirements,
                             std::vector<std::string>& dependencies,
                             std::unordered_set<std::string>& seen) {
        for (const std::string& requirement : requirements) {
            if (seen.emplace(requirement).second) {
                dependencies.push_back(requirement);
            }
        }
    }

    void append_requirements(const BuildConfiguration& configuration,
                             std::vector<std::string>& dependencies,
                             std::unordered_set<std::string>& seen) {
        for (const EnvironmentVariable& variable : configuration.environment) {
            append_requirements(variable.requires, dependencies, seen);
        }
        for (const BuildOption& option : configuration.options) {
            append_requirements(option.requires, dependencies, seen);
        }
    }
}  // namespace

std::vector<std::string> get_optional_dependencies(const std::string& package_name) {
    const PackageConfigPtr config = get_db_config(package_name);
    std::vector<std::string> dependencies;
    std::unordered_set<std::string> seen;

    if (!config->build.has_value()) {
        return dependencies;
    }
    if (config->build->configurations.has_value()) {
        append_requirements(*config->build->configurations, dependencies, seen);
    }
    for (const BuildStage& stage : config->build->stages) {
        if (stage.configurations.has_value()) {
            append_requirements(*stage.configurations, dependencies, seen);
        }
    }
    return dependencies;
}
