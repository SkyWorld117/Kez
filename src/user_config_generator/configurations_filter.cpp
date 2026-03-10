#include "configurations_filter.h"

YAML::Node filtered_configurations(const YAML::Node& config_node,
                                   const std::vector<std::string>& all_dependencies) {
    YAML::Node configs = YAML::Node(YAML::NodeType::Map);
    if (config_node["environment"]) {
        YAML::Node env = filtered_environment(config_node["environment"], all_dependencies);
        if (!env.IsNull() && !(env.size() == 0)) {
            configs["environment"] = env;
        }
    }
    if (config_node["options"]) {
        YAML::Node opts = filtered_options(config_node["options"], all_dependencies);
        if (!opts.IsNull() && !(opts.size() == 0)) {
            configs["options"] = opts;
        }
    }
    return configs;
}