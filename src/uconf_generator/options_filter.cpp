#include <dependency_resolver/requirements.hpp>
#include <uconf_generator/options_filter.hpp>

YAML::Node filtered_options(const std::vector<BuildOption>& options,
                            const std::vector<std::string>& all_dependencies,
                            const AbstractPackageSelections& abstract_packages) {
    YAML::Node result(YAML::NodeType::Sequence);
    for (const BuildOption& option : options) {
        // Skip options that are not user-configurable or whose requirements
        // are not met by the resolved dependency set.
        if (!option.user_configurable ||
            !requirements_satisfied(option.requires, all_dependencies, abstract_packages)) {
            continue;
        }

        YAML::Node output(YAML::NodeType::Map);
        output["name"] = option.name;
        if (option.description.has_value()) {
            output["description"] = *option.description;
        }

        // "enabled": default to true when the option has no conditional
        // enable/disable logic at all.
        if (!option.enabled.has_value()) {
            output["enabled"] = true;
        } else if (option.enabled->default_value.has_value()) {
            output["enabled"] = *option.enabled->default_value;
        }

        // "enabled_value": null when the option is a pure boolean flag
        // (no value to substitute into the format string).
        if (!option.enabled_value.has_value()) {
            output["enabled_value"] = YAML::Node(YAML::NodeType::Null);
        } else if (option.enabled_value->default_value.has_value()) {
            output["enabled_value"] = *option.enabled_value->default_value;
        }

        // "disabled_value": only emitted when the option supports a
        // disabled format (i.e. an explicit "off" representation).
        if (option.disabled_format.has_value()) {
            if (!option.disabled_value.has_value()) {
                output["disabled_value"] = YAML::Node(YAML::NodeType::Null);
            } else if (option.disabled_value->default_value.has_value()) {
                output["disabled_value"] = *option.disabled_value->default_value;
            }
        }

        // Expose the requirement list so downstream tooling knows which
        // packages must be present for this option to be valid.
        if (!option.requires.empty()) {
            output["requires"] = option.
                                     requires;
        }
        result.push_back(output);
    }
    return result;
}
