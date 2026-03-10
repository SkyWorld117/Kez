#include "environment_filter.h"

YAML::Node filtered_environment(const YAML::Node& env_node,
                                const std::vector<std::string>& all_dependencies) {
    YAML::Node env = YAML::Node(YAML::NodeType::Sequence);
    for (const auto& var : env_node) {
        if (var["user_configurable"] && var["user_configurable"].as<bool>()) {
            // Early abort if the environment is not enabled (insufficent condition)
            if (var["requires"]) {
                bool all_deps_present = true;
                for (const auto& dep : var["requires"]) {
                    if (std::find(all_dependencies.begin(), all_dependencies.end(),
                                  dep.as<std::string>()) == all_dependencies.end()) {
                        all_deps_present = false;
                        break;
                    }
                }
                if (!all_deps_present) {
                    continue;  // Skip this environment variable if not all required dependencies are present
                }
            }

            YAML::Node tmp_var = YAML::Node(YAML::NodeType::Map);
            for (const auto& key : var) {
                if (key.first.as<std::string>() == "default") {
                    tmp_var["value"] = key.second;
                } else if (key.first.as<std::string>() != "user_configurable" &&
                           key.first.as<std::string>() != "condition") {
                    tmp_var[key.first] = key.second;
                }
            }
            env.push_back(tmp_var);
        }
    }
    return env;
}