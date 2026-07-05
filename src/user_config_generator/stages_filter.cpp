#include <user_config_generator/configurations_filter.hpp>
#include <user_config_generator/stages_filter.hpp>

YAML::Node filtered_stages(const std::vector<BuildStage>& stages,
                           const std::vector<std::string>& all_dependencies,
                           const AbstractPackageSelections& abstract_packages) {
    YAML::Node result(YAML::NodeType::Sequence);
    for (const BuildStage& stage : stages) {
        if (!stage.configurations.has_value()) {
            continue;
        }

        YAML::Node configurations =
            filtered_configurations(*stage.configurations, all_dependencies, abstract_packages);
        if (configurations.size() == 0) {
            continue;
        }

        YAML::Node output(YAML::NodeType::Map);
        if (stage.target.has_value()) {
            output["target"] = *stage.target;
        } else {
            output["target"] = YAML::Node(YAML::NodeType::Null);
        }
        output["configurations"] = configurations;
        result.push_back(output);
    }
    return result;
}
