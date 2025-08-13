#include "stages_filter.h"

YAML::Node filtered_stages(const YAML::Node& stages_node,
                           const std::vector<std::string>& all_dependencies) {
    YAML::Node stages = YAML::Node(YAML::NodeType::Sequence);
    for (const auto& stage : stages_node) {
        if (stage["configurations"]) {
            YAML::Node filtered = filtered_configurations(stage["configurations"], all_dependencies);
            if (!filtered.IsNull() && !(filtered.size() == 0)) {
                YAML::Node tmp_stage = YAML::Node(YAML::NodeType::Map);
                tmp_stage["target"] = stage["target"];
                tmp_stage["configurations"] = filtered;
                stages.push_back(tmp_stage);
            }
        }
    }
    return stages;
}