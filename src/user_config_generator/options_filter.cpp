#include "options_filter.h"

YAML::Node filtered_options(const YAML::Node&               options_node,
                            const std::vector<std::string>& all_dependencies) {
    YAML::Node opts = YAML::Node(YAML::NodeType::Sequence);
    for (const auto& opt : options_node) {
        if (opt["user_configurable"] && opt["user_configurable"].as<bool>()) {
            // Early abort if the option is not enabled (insufficient condition)
            if (opt["requires"]) {
                bool all_deps_present = true;
                for (const auto& dep : opt["requires"]) {
                    if (std::find(all_dependencies.begin(), all_dependencies.end(),
                                  dep.as<std::string>()) == all_dependencies.end()) {
                        all_deps_present = false;
                        break;
                    }
                }
                if (!all_deps_present) {
                    continue;  // Skip this option if not all required dependencies are present
                }
            }

            YAML::Node tmp_opt = YAML::Node(YAML::NodeType::Map);

            tmp_opt["name"] = opt["name"];

            if (opt["description"]) {
                tmp_opt["description"] = opt["description"];
            }

            if (opt["enabled"]) {
                if (opt["enabled"]["default"]) {
                    tmp_opt["enabled"] = opt["enabled"]["default"];
                }
                // Otherwise probably determined via condition, do not touch it then.
            } else {
                tmp_opt["enabled"] = true;  // Default to enabled if not specified
            }

            if (opt["enabled_value"]) {
                if (opt["enabled_value"]["default"]) {
                    tmp_opt["enabled_value"] = opt["enabled_value"]["default"];
                }
            } else {
                tmp_opt["enabled_value"] =
                    YAML::Node(YAML::NodeType::Null);  // Default to null if not specified
            }

            if (opt["disabled_format"]) {
                if (opt["disabled_value"]) {
                    if (opt["disabled_value"]["default"]) {
                        tmp_opt["disabled_value"] = opt["disabled_value"]["default"];
                    }
                } else {
                    tmp_opt["disabled_value"] =
                        YAML::Node(YAML::NodeType::Null);  // Default to null if not specified
                }
            }

            if (opt["requires"]) {
                tmp_opt["requires"] = opt["requires"];
            }

            opts.push_back(tmp_opt);
        }
    }
    return opts;
}