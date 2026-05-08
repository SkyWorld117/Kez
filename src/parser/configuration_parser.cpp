#include <parser/configuration_parser.hpp>

void parse_configuration(std::vector<std::string>& instructions,
                         const std::string& command,
                         YAML::Node& config,
                         const std::string& toolchain,
                         ParserContext& context) {
    // Unpack context
    const YAML::Node user_config_context = context.user_config_context;

    std::unordered_map<std::string, std::string> env_config;
    if (config["environment"]) {
        YAML::Node user_config_context_env;
        if (user_config_context["environment"]) {
            user_config_context_env = user_config_context["environment"];
        }
        context.user_config_context.reset(user_config_context_env);

        env_config = parse_environment(config["environment"], context);
    }

    std::string opts_config;
    if (config["options"]) {
        YAML::Node user_config_context_opts;
        if (user_config_context["options"]) {
            user_config_context_opts = user_config_context["options"];
        }
        context.user_config_context.reset(user_config_context_opts);

        opts_config = parse_options(config["options"], toolchain, context);
    }

    // Reset user config context
    context.user_config_context.reset(user_config_context);

    // Construct command
    std::string cmd = command;
    if (!opts_config.empty()) {
        cmd += " " + opts_config;
    }

    // Wrap with environment variables
    std::unordered_map<std::string, std::string> env_map;
    for (const auto& [key, value] : env_config) {
        std::string env_value = getenv(key.c_str()) ? getenv(key.c_str()) : "";
        env_map[key] = env_value;
        instructions.push_back("export " + key + "=\"" + value + "\"");
    }

    // Add command to instructions
    instructions.push_back(cmd);

    // Unset environment variables after command
    for (const auto& [key, value] : env_config) {
        if (env_map[key].empty()) {
            instructions.push_back("unset " + key);
        } else {
            instructions.push_back("export " + key + "=\"" + env_map[key] + "\"");
        }
    }
}