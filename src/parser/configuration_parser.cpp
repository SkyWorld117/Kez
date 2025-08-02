#include "configuration_parser.h"

std::pair<std::vector<std::string>, std::string> parse_configuration(
    YAML::Node& config,
    std::unordered_map<std::string, std::string>& template_map,
    const YAML::Node& user_config,
    const YAML::Node& user_config_context,
    const YAML::Node& pkg_config,
    const std::string& build_mode,
    const std::string& env_path
) {
    std::vector<std::string> env_config;
    if (config["environment"]) {
        YAML::Node user_config_context_env;
        if (user_config_context["environment"]) {
            user_config_context_env = user_config_context["environment"];
        }
        env_config = parse_environment(
            config["environment"],
            template_map,
            user_config,
            user_config_context_env,
            pkg_config,
            build_mode,
            env_path
        );
    } else {
        env_config = {};
    }

    std::string opts_config;
    if (config["options"]) {
        YAML::Node user_config_context_opts;
        if (user_config_context["options"]) {
            user_config_context_opts = user_config_context["options"];
        }
        opts_config = parse_options(
            config["options"],
            template_map,
            user_config,
            user_config_context_opts,
            pkg_config,
            build_mode,
            env_path
        );
    } else {
        opts_config = "";
    }

    return { env_config, opts_config };
}