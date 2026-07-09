#include <user_config_generator/configurations_filter.hpp>
#include <user_config_generator/stages_filter.hpp>

/** @brief Prune build stages whose per-stage configurations have no satisfied
 *         dependencies, emitting only stages with at least one surviving entry. */
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
