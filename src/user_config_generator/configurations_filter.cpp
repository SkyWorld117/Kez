#include <user_config_generator/configurations_filter.hpp>
#include <user_config_generator/environment_filter.hpp>
#include <user_config_generator/options_filter.hpp>

/**
 * @brief Filters a BuildConfiguration down to only those entries whose
 *        associated dependency or abstract-package conditions are met.
 *
 * This is the top-level entry point for configuration pruning. It delegates
 * to `filtered_environment()` and `filtered_options()` to independently
 * filter the environment variables and the build options sections of the
 * given configuration.  A section is only included in the output map when
 * the corresponding filter returns a non-empty YAML node.
 *
 * @param configuration      The full BuildConfiguration to be filtered. Its
 *                           `environment` and `options` members are each
 *                           processed independently.
 * @param all_dependencies   Flat list of concrete package names that have
 *                           been selected for installation.  Used by the
 *                           sub-filters to decide which conditionally-gated
 *                           entries to keep.
 * @param abstract_packages  Map from abstract package name (e.g. "BLAS",
 *                           "MPI") to the concrete package that satisfies
 *                           it.  Used by the sub-filters to resolve
 *                           abstract-package conditions.
 *
 * @return A YAML::Node (of type Map) containing at most two keys:
 *         - "environment"  — present if filtered_environment returned
 *                            non-empty,
 *         - "options"      — present if filtered_options returned non-empty.
 *         Returns an empty map when nothing survives filtering (both
 *         sub-filters produce empty nodes).
 *
 * @note No error handling is performed directly at this level; the sub-
 *       filter functions (`filtered_environment` and `filtered_options`)
 *       are responsible for reporting any configuration-level errors via
 *       `user_config_error` or similar termination helpers.
 */
YAML::Node filtered_configurations(const BuildConfiguration& configuration,
                                   const std::vector<std::string>& all_dependencies,
                                   const AbstractPackageSelections& abstract_packages) {
    YAML::Node result(YAML::NodeType::Map);

    YAML::Node environment =
        filtered_environment(configuration.environment, all_dependencies, abstract_packages);
    if (environment.size() != 0) {
        result["environment"] = environment;
    }

    YAML::Node options =
        filtered_options(configuration.options, all_dependencies, abstract_packages);
    if (options.size() != 0) {
        result["options"] = options;
    }
    return result;
}
