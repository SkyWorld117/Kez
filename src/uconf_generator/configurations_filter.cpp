#include <uconf_generator/configurations_filter.hpp>
#include <uconf_generator/environment_filter.hpp>
#include <uconf_generator/options_filter.hpp>

YAML::Node filtered_configurations(const BuildConfiguration& configuration,
                                   const std::vector<std::string>& all_dependencies,
                                   const AbstractPackageSelections& abstract_packages) {
    YAML::Node result(YAML::NodeType::Map);

    YAML::Node environment =
        filtered_environment(configuration.environment, all_dependencies, abstract_packages);
    if (environment.size() != 0) {
        result["environment"] = environment;
    }

    YAML::Node options =
        filtered_options(configuration.options, all_dependencies, abstract_packages);
    if (options.size() != 0) {
        result["options"] = options;
    }
    return result;
}
