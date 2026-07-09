#include <user_config_generator/configurations_filter.hpp>
#include <user_config_generator/stages_filter.hpp>

/**
 * @brief Filter a list of build stages, retaining only those whose
 *        dependency-constrained configurations are non-empty.
 *
 * Iterates over every BuildStage in the input vector and applies two
 * successive filters:
 *   1. Stages that have no `configurations` (i.e. `std::nullopt`) are
 *      skipped immediately -- they contribute nothing to the output.
 *   2. Stages whose `configurations` are present are passed to
 *      filtered_configurations(), which removes environment variables and
 *      build options whose dependency requirements are not satisfied by the
 *      resolved dependency set.  If *all* entries in the configuration are
 *      pruned, the stage is omitted entirely.
 *
 * For each surviving stage, a YAML map with two keys is emitted:
 *   - "target"        -- the stage's optional target string, or YAML `null`
 *                        if not set.
 *   - "configurations" -- the filtered configuration node returned by
 *                         filtered_configurations().
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
 * @note  This function performs no error reporting or termination --
 *        stages with missing or fully-pruned configurations are silently
 *        omitted.  The caller is expected to handle an empty result
 *        (e.g. by falling back to toolchain-default stages).
 *
 * @see filtered_configurations() in configurations_filter.hpp
 * @see BuildStage                in database/config.hpp
 * @see BuildConfiguration        in database/config.hpp
 * @see AbstractPackageSelections in dependency_resolver/resolve_dependencies.hpp
 */
YAML::Node filtered_stages(const std::vector<BuildStage>& stages,
                           const std::vector<std::string>& all_dependencies,
                           const AbstractPackageSelections& abstract_packages) {
    YAML::Node result(YAML::NodeType::Sequence);
    for (const BuildStage& stage : stages) {
        // Skip stages that carry no per-stage configuration at all;
        // they have nothing to filter and cannot contribute to the output.
        if (!stage.configurations.has_value()) {
            continue;
        }

        // Delegate to filtered_configurations() which prunes environment
        // variables and build options whose dependency requirements are
        // not satisfied by the resolved dependency set.
        YAML::Node configurations =
            filtered_configurations(*stage.configurations, all_dependencies, abstract_packages);
        if (configurations.size() == 0) {
            // Every entry in this stage's configuration was pruned;
            // emitting an empty configurations block would serve no purpose.
            continue;
        }

        YAML::Node output(YAML::NodeType::Map);
        if (stage.target.has_value()) {
            output["target"] = *stage.target;
        } else {
            // Emit an explicit YAML null so downstream consumers can
            // distinguish "unset target" from a missing key.
            output["target"] = YAML::Node(YAML::NodeType::Null);
        }
        output["configurations"] = configurations;
        result.push_back(output);
    }
    return result;
}
