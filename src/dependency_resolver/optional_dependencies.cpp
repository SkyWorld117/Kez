#include <database/config.hpp>
#include <database/database.hpp>
#include <dependency_resolver/optional_dependencies.hpp>
#include <unordered_set>

namespace {
    /**
     * @brief Append unique requirement strings to a dependency list.
     *
     * Iterates over @p requirements and copies each entry into @p dependencies
     * that has not already been inserted (tracked via @p seen).  This is the
     * leaf-level overload that actually populates the output vector.
     *
     * @param requirements  Zero or more package-name strings to consider
     *                      adding.  Each corresponds to a package whose
     *                      properties must be resolvable before the owning
     *                      environment variable or build option can be evaluated.
     * @param dependencies  Output vector to which previously-unseen entries
     *                      are appended.  The relative order from @p requirements
     *                      is preserved for first-time entries.
     * @param seen          Set of package names already present in
     *                      @p dependencies (or already processed earlier in
     *                      the overall traversal).  Used to suppress duplicates.
     */
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

std::vector<std::string> get_optional_dependencies(const PackageConfig& config) {
    std::vector<std::string> dependencies;
    std::unordered_set<std::string> seen;

    if (!config.build.has_value()) {
        return dependencies;
    }
    if (config.build->configurations.has_value()) {
        append_requirements(*config.build->configurations, dependencies, seen);
    }
    for (const BuildStage& stage : config.build->stages) {
        if (stage.configurations.has_value()) {
            append_requirements(*stage.configurations, dependencies, seen);
        }
    }
    return dependencies;
}

std::vector<std::string> get_optional_dependencies(const std::string& package_name) {
    return get_optional_dependencies(*get_db_config(package_name));
}
