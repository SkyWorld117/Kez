#include <parser/configuration_parser.hpp>

std::pair<std::vector<std::string>, std::string> parse_configuration(YAML::Node& config,
                                                                     const std::string& toolchain,
                                                                     ParserContext& context) {
    // Unpack context
    const YAML::Node user_config_context = context.user_config_context;

    std::vector<std::string> env_config;
    if (config["environment"]) {
        YAML::Node user_config_context_env;
        if (user_config_context["environment"]) {
            user_config_context_env = user_config_context["environment"];
        }
        context.user_config_context.reset(user_config_context_env);

        env_config = parse_environment(config["environment"], context);
    } else {
        env_config = {};
    }

    std::string opts_config;
    if (config["options"]) {
        YAML::Node user_config_context_opts;
        if (user_config_context["options"]) {
            user_config_context_opts = user_config_context["options"];
        }
        context.user_config_context.reset(user_config_context_opts);

        opts_config = parse_options(config["options"], toolchain, context);
    } else {
        opts_config = "";
    }

    // Reset user config context
    context.user_config_context.reset(user_config_context);

    return {env_config, opts_config};
}