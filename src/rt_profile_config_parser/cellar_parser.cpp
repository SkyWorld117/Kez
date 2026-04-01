#include <rt_profile_config_parser/cellar_parser.hpp>

YAML::Node parse_cellar_config(const YAML::Node& factory_config, const YAML::Node& cellar_config) {
    if (!cellar_config.IsMap()) {
        ERROR("Cellar configuration should be a map");
        exit(EXIT_FAILURE);
    }

    if (!cellar_config["name"] || !cellar_config["name"].IsScalar()) {
        ERROR("Cellar configuration missing 'name' key or 'name' is not a scalar");
        exit(EXIT_FAILURE);
    }

    if (cellar_config["sibling"]) {
        if (!cellar_config["sibling"].IsScalar()) {
            ERROR("'sibling' key should be a scalar");
            exit(EXIT_FAILURE);
        }

        std::string sibling_name = cellar_config["sibling"].as<std::string>();
        for (const auto& sibling_cellar : factory_config["cellars"]) {
            if (!sibling_cellar.IsMap() || !sibling_cellar["name"] ||
                !sibling_cellar["name"].IsScalar()) {
                continue;  // Skip invalid sibling entries
            }

            if (sibling_cellar["name"].as<std::string>() == sibling_name) {
                return parse_cellar_config(factory_config, sibling_cellar);
            }
        }
    }

    if (!cellar_config["profiles"]) {
        ERROR("Cellar configuration missing 'profiles' key");
        exit(EXIT_FAILURE);
    }

    if (!cellar_config["profiles"].IsSequence()) {
        ERROR("'profiles' key should be a sequence");
        exit(EXIT_FAILURE);
    }

    YAML::Node profile_instructions(YAML::NodeType::Sequence);
    for (const auto& profile_config : cellar_config["profiles"]) {
        YAML::Node run_instructions(YAML::NodeType::Map);
        run_instructions["name"] = profile_config["name"].as<std::string>();
        INFO("  Parsing profile: " + profile_config["name"].as<std::string>());
        auto [instructions, summary_regex] =
            parse_run_config(factory_config, cellar_config, profile_config);
        run_instructions["instructions"]  = instructions;
        run_instructions["summary_regex"] = summary_regex;
        profile_instructions.push_back(run_instructions);
    }
    return profile_instructions;
}