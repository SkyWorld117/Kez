#pragma once

#include <yaml-cpp/yaml.h>

#include <database/config.hpp>
#include <dependency_resolver/resolve_dependencies.hpp>
#include <string>
#include <vector>

/**
 * @brief Filter a list of build stages, retaining only those whose
 *        dependency-constrained configurations are non-empty.
 *
 * Iterates over every BuildStage in the input vector and applies two
 * successive filters:
 *   1. Stages that have no `configurations` (i.e. `std::nullopt`) are
 *      skipped immediately — they contribute nothing to the output.
 *   2. Stages whose `configurations` are present are passed to
 *      filtered_configurations(), which removes environment variables and
 *      build options whose dependency requirements are not satisfied by the
 *      resolved dependency set.  If *all* entries in the configuration are
 *      pruned, the stage is omitted entirely.
 *
 * For each surviving stage, a YAML map with two keys is emitted:
 *   - "target"        – the stage's optional target string, or YAML `null`
 *                       if not set.
 *   - "configurations"– the filtered configuration node returned by
 *                       filtered_configurations().
 *
 * The result is a YAML sequence in the original stage order.  An empty
 * sequence is returned when no stages pass the filter.
 *
 * @param stages              The complete list of build stages defined for a
 *                            package recipe.  Each stage carries an optional
 *                            target name and an optional BuildConfiguration.
 * @param all_dependencies    The full list of resolved package names (concrete
 *                            dependencies) that will be available in the
 *                            current installation plan.  Used to determine
 *                            whether a stage's configuration entries are
 *                            applicable.
 * @param abstract_packages   Mapping from abstract package names
 *                            (e.g. "blas", "mpi") to the concrete package
 *                            selected for the current architecture.
 *                            Abstract references in a configuration entry's
 *                            `requires` list are resolved through this map
 *                            before the availability check is performed.
 *
 * @return A YAML::Node of type Sequence.  Each element is a Map with keys
 *         "target" and "configurations", in the order they appeared in
 *         `stages`.  Returns an empty Sequence when no stages survive
 *         filtering.
 *
 * @see filtered_configurations() in configurations_filter.hpp
 * @see BuildStage                in database/config.hpp
 * @see BuildConfiguration        in database/config.hpp
 * @see AbstractPackageSelections in dependency_resolver/resolve_dependencies.hpp
 */
YAML::Node filtered_stages(const std::vector<BuildStage>& stages,
                           const std::vector<std::string>& all_dependencies,
                           const AbstractPackageSelections& abstract_packages);
