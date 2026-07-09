#pragma once

#include <yaml-cpp/yaml.h>

#include <database/config.hpp>
#include <dependency_resolver/resolve_dependencies.hpp>
#include <string>
#include <vector>

/**
 * @brief Filter environment variables against a resolved dependency list with
 *        abstract-package resolution.
 *
 * Iterates over the given @p environment variables and retains only those whose
 * `requires` constraints are satisfied by the set of resolved concrete
 * dependencies. When an environment variable's `requires` list names an
 * abstract package (e.g. "BLAS"), the @p abstract_packages map is consulted to
 * determine whether the resolved selection for that abstract satisfies the
 * constraint. Variables with no `requires` entries are always included.
 *
 * Each surviving variable is emitted as a YAML mapping node with its @c name,
 * @c value, and (if present) @c description and @c user_configurable fields.
 * The @c value is resolved through any conditional branches against concrete
 * dependency names before emission.
 *
 * @param environment         The list of EnvironmentVariable definitions to
 *                            filter (typically sourced from a package's build
 *                            configuration).
 * @param all_dependencies    The complete resolved list of concrete dependency
 *                            names (package, system, compiler, MPI, vendor,
 *                            etc.) that are scheduled for installation.
 * @param abstract_packages  Mapping from abstract package names (e.g. "BLAS",
 *                            "LAPACK", "MPI") to the concrete package selected
 *                            by the dependency resolver's advisor. Used to
 *                            match abstract constraints in environment
 *                            variable requirements.
 *
 * @return A YAML::Node containing a sequence of filtered environment-variable
 *         mappings, ready for serialization into the user-facing configuration
 *         YAML.
 *
 * @see filtered_environment(const std::vector<EnvironmentVariable>&,
 *                           const std::vector<std::string>&)  Convenience
 *                           overload that omits abstract-package resolution.
 */
YAML::Node filtered_environment(const std::vector<EnvironmentVariable>& environment,
                                const std::vector<std::string>& all_dependencies,
                                const AbstractPackageSelections& abstract_packages);

/**
 * @brief Filter environment variables against a resolved dependency list
 *        without abstract-package resolution.
 *
 * Convenience overload that delegates to the three-parameter version with an
 * empty abstract-package map. Use this when the caller has already resolved
 * all abstract names to concrete entries in @p all_dependencies, or when no
 * abstract packages appear in the environment variable constraints.
 *
 * Variables whose `requires` list contains only concrete dependency names are
 * evaluated identically to the three-parameter form. If an abstract name does
 * appear in a constraint it will never match (the empty map is consulted), so
 * that variable is dropped.
 *
 * @param environment       The list of EnvironmentVariable definitions to
 *                          filter.
 * @param all_dependencies  The complete resolved list of concrete dependency
 *                          names scheduled for installation.
 *
 * @return A YAML::Node containing a sequence of filtered environment-variable
 *         mappings, ready for serialization into the user-facing configuration
 *         YAML.
 *
 * @see filtered_environment(const std::vector<EnvironmentVariable>&,
 *                           const std::vector<std::string>&,
 *                           const AbstractPackageSelections&)  Full
 *                           overload with abstract-package resolution support.
 */
YAML::Node filtered_environment(const std::vector<EnvironmentVariable>& environment,
                                const std::vector<std::string>& all_dependencies);
