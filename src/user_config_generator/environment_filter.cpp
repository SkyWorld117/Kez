#include <dependency_resolver/requirements.hpp>
#include <user_config_generator/environment_filter.hpp>

/**
 * @brief Filter environment variables against a resolved dependency list with
 *        abstract-package resolution.
 *
 * Iterates over the full list of @p environment variables and retains only
 * those that satisfy **both** conditions:
 *   1. The variable is marked as @c user_configurable (i.e. end-users are
 *      allowed to override it from their top-level configuration YAML).
 *   2. Every name in the variable's @c requires list is present (directly or
 *      via abstract-package mapping) in @p all_dependencies.
 *
 * Variables that fail either condition are silently skipped.  Each surviving
 * variable is emitted as a YAML mapping node containing:
 *   - @c name          -- the variable's name (required).
 *   - @c description   -- the human-readable description, if one exists.
 *   - @c value         -- the variable's default value, if one is specified.
 *   - @c requires      -- the dependency-constraint list, if non-empty.
 *
 * @param environment        The full list of environment variable definitions
 *                           to filter.  Typically sourced from a package's
 *                           build configuration (see @ref
 *                           BuildConfiguration::environment).
 * @param all_dependencies   The complete resolved list of concrete dependency
 *                           names (package, system, compiler, MPI, vendor,
 *                           etc.) scheduled for installation.  Used to
 *                           determine whether a variable's @c requires
 *                           constraints are met.
 * @param abstract_packages  Mapping from abstract package names (e.g. "BLAS",
 *                           "LAPACK", "MPI") to the concrete implementation
 *                           selected during dependency resolution.  Consulted
 *                           when a variable's @c requires entry names an
 *                           abstract package.
 *
 * @return A YAML::Node containing a sequence of mappings, one per surviving
 *         environment variable.  The sequence is ready for direct
 *         serialization into the user-facing configuration YAML.
 *
 * @note This function does **not** terminate the program.  Variables with
 *       unsatisfied requirements are silently omitted rather than raising an
 *       error.
 *
 * @see filtered_environment(const std::vector<EnvironmentVariable>&,
 *                           const std::vector<std::string>&)
 *     Convenience overload that delegates to this function with an empty
 *     abstract-package map.
 */
YAML::Node filtered_environment(const std::vector<EnvironmentVariable>& environment,
                                const std::vector<std::string>& all_dependencies,
                                const AbstractPackageSelections& abstract_packages) {
    YAML::Node result(YAML::NodeType::Sequence);
    for (const EnvironmentVariable& variable : environment) {
        if (!variable.user_configurable ||
            !requirements_satisfied(variable.requires, all_dependencies, abstract_packages)) {
            continue;
        }

        YAML::Node output(YAML::NodeType::Map);
        output["name"] = variable.name;
        if (variable.description.has_value()) {
            output["description"] = *variable.description;
        }
        if (variable.value.default_value.has_value()) {
            output["value"] = *variable.value.default_value;
        }
        if (!variable.requires.empty()) {
            output["requires"] = variable.
                                     requires;
        }
        result.push_back(output);
    }
    return result;
}

/**
 * @brief Filter environment variables against a resolved dependency list
 *        without abstract-package resolution.
 *
 * Convenience overload that delegates to the three-parameter version with an
 * empty abstract-package map (i.e. no abstract-to-concrete resolution is
 * performed).  Use this when all names appearing in environment variable
 * @c requires lists are concrete dependency names already present in
 * @p all_dependencies.
 *
 * If a variable's @c requires list contains an abstract package name (e.g.
 * "BLAS") for which no concrete mapping is provided, the requirement will
 * never match and that variable is silently dropped.
 *
 * @param environment       The list of environment variable definitions to
 *                          filter.
 * @param all_dependencies  The complete resolved list of concrete dependency
 *                          names scheduled for installation.
 *
 * @return A YAML::Node containing a sequence of filtered environment-variable
 *         mappings, ready for serialization into the user-facing configuration
 *         YAML.
 *
 * @note This function does **not** terminate the program.  Variables with
 *       unsatisfied requirements are silently omitted.
 *
 * @see filtered_environment(const std::vector<EnvironmentVariable>&,
 *                           const std::vector<std::string>&,
 *                           const AbstractPackageSelections&)
 *     The full overload that supports abstract-package resolution.
 */
YAML::Node filtered_environment(const std::vector<EnvironmentVariable>& environment,
                                const std::vector<std::string>& all_dependencies) {
    return filtered_environment(environment, all_dependencies, {});
}
