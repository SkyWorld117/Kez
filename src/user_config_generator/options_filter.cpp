#include <user_config_generator/options_filter.hpp>
#include <user_config_generator/requirements_filter.hpp>

YAML::Node filtered_options(const std::vector<BuildOption>& options,
                            const std::vector<std::string>& all_dependencies,
                            const AbstractPackageSelections& abstract_packages) {
    YAML::Node result(YAML::NodeType::Sequence);
    for (const BuildOption& option : options) {
        if (!option.user_configurable ||
            !requirements_satisfied(option.requires, all_dependencies, abstract_packages)) {
            continue;
        }

        YAML::Node output(YAML::NodeType::Map);
        output["name"] = option.name;
        if (option.description.has_value()) {
            output["description"] = *option.description;
        }

        if (!option.enabled.has_value()) {
            output["enabled"] = true;
        } else if (option.enabled->default_value.has_value()) {
            output["enabled"] = *option.enabled->default_value;
        }

        if (!option.enabled_value.has_value()) {
            output["enabled_value"] = YAML::Node(YAML::NodeType::Null);
        } else if (option.enabled_value->default_value.has_value()) {
            output["enabled_value"] = *option.enabled_value->default_value;
        }

        if (option.disabled_format.has_value()) {
            if (!option.disabled_value.has_value()) {
                output["disabled_value"] = YAML::Node(YAML::NodeType::Null);
            } else if (option.disabled_value->default_value.has_value()) {
                output["disabled_value"] = *option.disabled_value->default_value;
            }
        }

        if (!option.requires.empty()) {
            output["requires"] = option.
                                     requires;
        }
        result.push_back(output);
    }
    return result;
}
