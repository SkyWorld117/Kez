#include <rt_profile_config_parser/factory_parser.hpp>

YAML::Node parse_factory_config(const YAML::Node& factory_config) {
    if (!factory_config.IsMap()) {
        ERROR("Factory configuration should be a map");
        exit(EXIT_FAILURE);
    }

    if (!factory_config["cellars"].IsSequence()) {
        ERROR("'cellars' key should be a sequence");
        exit(EXIT_FAILURE);
    }

    YAML::Node instructions_yaml(YAML::NodeType::Sequence);
    for (const auto& item : factory_config["cellars"]) {
        YAML::Node cellar_instructions(YAML::NodeType::Map);
        cellar_instructions["name"] = item["name"].as<std::string>();
        INFO("Parsing cellar: " + item["name"].as<std::string>());
        cellar_instructions["profiles"] = parse_cellar_config(factory_config, item);
        instructions_yaml.push_back(cellar_instructions);
    }
    return instructions_yaml;
}