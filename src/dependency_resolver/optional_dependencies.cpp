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

    /**
     * @brief Extract all unique requirement package names from a
     *        BuildConfiguration.
     *
     * Walks the configuration's @c environment variables and @c options,
     * collecting the union of their respective @c requires fields.  Each
     * resulting package name is appended to @p dependencies at most once
     * (deduplication is handled via the @p seen set, which is shared across
     * all call sites in the overall traversal).
     *
     * @param configuration  A build configuration whose environment variables
     *                       and options are scanned.  Only the @c requires
     *                       sub-field of each variable and option is examined;
     *                       the actual values and conditions are ignored.
     * @param dependencies   Output vector accumulating unique package names.
     * @param seen           Shared deduplication set; entries already present
     *                       (from any prior call) will be skipped.
     */
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

/**
 * @brief Collect all optional (conditional) dependencies for a given package.
 *
 * Queries the package's database configuration and extracts the union of all
 * package names listed in the @c requires fields of:
 *   - Environment variables defined at the build level
 *   - Build options defined at the build level
 *   - Environment variables defined in each build stage's configurations
 *   - Build options defined in each build stage's configurations
 *
 * These are "optional" dependencies because they are only needed when the
 * corresponding environment variable or build option is actually enabled by
 * the user in the generated configuration file -- as opposed to hard
 * dependencies listed in the package's top-level @c dependencies field.
 *
 * Duplicates are removed from the returned list.  The order among unique
 * entries is roughly the order they first appear during the traversal
 * (build-level configs first, then stage-level configs).
 *
 * @param package_name  The name of the package whose optional dependencies
 *                      should be retrieved (corresponds to a subdirectory
 *                      under the project's @c database/ directory).
 *
 * @return A vector of zero or more unique package names that are optional
 *         dependencies of the specified package.  An empty vector is returned
 *         if the package does not exist in the database or if its recipe
 *         defines no optional dependencies.
 *
 * @note   This function does **not** terminate the program on its own, but
 *         the underlying call to get_db_config() will abort (via @c ERROR)
 *         if @p package_name is invalid, the recipe file is missing, or the
 *         YAML is malformed.
 *
 * @see get_db_config()            Source of the package configuration data.
 * @see EnvironmentVariable        Struct whose @c requires field contributes
 *                                 optional dependency entries.
 * @see BuildOption                Struct whose @c requires field contributes
 *                                 optional dependency entries.
 * @see BuildConfiguration         Aggregates environment variables and options
 *                                 that carry @c requires fields.
 * @see BuildStage                 May contain a @c BuildConfiguration with
 *                                 additional @c requires entries.
 */
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
