#pragma once

#include <yaml-cpp/yaml.h>

#include <database/config.hpp>
#include <dependency_resolver/resolve_dependencies.hpp>
#include <string>
#include <vector>

/**
 * @brief Filter a BuildConfiguration's environment variables and options,
 *        returning only the entries whose dependency requirements are satisfied.
 *
 * Iterates over the environment variables and build options declared in the
 * given BuildConfiguration and retains only those whose `requires` lists are
 * fully met by the set of resolved dependencies.  Abstract package references
 * in `requires` are resolved through the provided selections map before the
 * check is performed.
 *
 * The filtering is delegated to:
 *   - filtered_environment()  (from environment_filter.hpp)
 *   - filtered_options()      (from options_filter.hpp)
 *
 * A section (environment or options) is omitted from the returned YAML map
 * when every entry in that section was filtered out.
 *
 * @param configuration       The build configuration whose environment
 *                            variables and options are to be filtered.
 * @param all_dependencies    The full list of resolved package names
 *                            (concrete dependencies) available in the
 *                            current installation plan.
 * @param abstract_packages   Mapping from abstract package names
 *                            (e.g. "blas", "mpi") to the concrete package
 *                            selected for the current architecture.
 *                            Used to resolve abstract dependency references
 *                            before checking availability.
 *
 * @return A YAML::Node of type Map.  Contains zero, one, or two keys:
 *         - "environment" (YAML::Node of type Sequence) – present only if at
 *           least one environment variable survives the filter.
 *         - "options"     (YAML::Node of type Sequence) – present only if at
 *           least one build option survives the filter.
 *         Returns an empty Map when no entries pass filtering.
 *
 * @see filtered_environment() in environment_filter.hpp
 * @see filtered_options()     in options_filter.hpp
 * @see BuildConfiguration     in database/config.hpp
 * @see AbstractPackageSelections in dependency_resolver/resolve_dependencies.hpp
 */
YAML::Node filtered_configurations(const BuildConfiguration& configuration,
                                   const std::vector<std::string>& all_dependencies,
                                   const AbstractPackageSelections& abstract_packages);
