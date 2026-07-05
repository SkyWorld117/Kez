#include <dependency_resolver/requirements.hpp>
#include <user_config_generator/environment_filter.hpp>

YAML::Node filtered_environment(const std::vector<EnvironmentVariable>& environment,
                                const std::vector<std::string>& all_dependencies,
                                const AbstractPackageSelections& abstract_packages) {
    YAML::Node result(YAML::NodeType::Sequence);
    for (const EnvironmentVariable& variable : environment) {
        if (!variable.user_configurable ||
            !requirements_satisfied(variable.requires, all_dependencies, abstract_packages)) {
            continue;
        }

        YAML::Node output(YAML::NodeType::Map);
        output["name"] = variable.name;
        if (variable.description.has_value()) {
            output["description"] = *variable.description;
        }
        if (variable.value.default_value.has_value()) {
            output["value"] = *variable.value.default_value;
        }
        if (!variable.requires.empty()) {
            output["requires"] = variable.
                                     requires;
        }
        result.push_back(output);
    }
    return result;
}

YAML::Node filtered_environment(const std::vector<EnvironmentVariable>& environment,
                                const std::vector<std::string>& all_dependencies) {
    return filtered_environment(environment, all_dependencies, {});
}
