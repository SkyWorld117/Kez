#include <parser/environment_parser.hpp>
#include <unordered_map>

std::unordered_map<std::string, std::string> parse_environment(const YAML::Node& env_node,
                                                               ParserContext& context) {
    // Unpack context
    const YAML::Node user_config                               = context.user_config;
    const YAML::Node user_config_context                       = context.user_config_context;
    const YAML::Node pkg_config                                = context.pkg_config;
    std::unordered_map<std::string, std::string>& template_map = context.template_map;

    std::string pkg_name = pkg_config["cheese"]["name"].as<std::string>();
    std::unordered_map<std::string, std::string> env_vars;

    for (YAML::Node var : env_node) {
        DEBUG("  - Parsing environment variable: " + var["name"].as<std::string>());
        std::string var_name = var["name"].as<std::string>();
        std::string base_value;

        bool is_required = true;
        if (var["requires"]) {
            std::vector<std::string> all_dependencies =
                user_config["recipe"]["dependencies"].as<std::vector<std::string>>();
            for (const auto& dep : var["requires"]) {
                if (std::find(all_dependencies.begin(), all_dependencies.end(),
                              dep.as<std::string>()) == all_dependencies.end()) {
                    is_required = false;
                    break;
                }
            }
        }
        if (var["user_configurable"] && var["user_configurable"].as<bool>() && is_required) {
            YAML::Node user_config_var;
            for (YAML::Node user_var : user_config_context) {
                if (user_var["name"].as<std::string>() == var_name) {
                    user_config_var = user_var;
                    break;
                }
            }
            if (user_config_var["value"].IsNull()) {
                base_value = "";
            } else {
                base_value = user_config_var["value"].as<std::string>();
            }
        } else {
            if (!var["default"] || var["default"].IsNull()) {
                base_value = "";
            } else {
                base_value = var["default"].as<std::string>();
            }
        }

        std::string value;
        if (var["conditions"]) {
            value = parse_conditions(base_value, var["conditions"], context);
        } else {
            value = base_value;
        }

        if (!value.empty()) {
            // Resolve templates in the value
            value              = parse_scalar(value, context);
            env_vars[var_name] = value;
        }

        template_map[pkg_name + ".env." + var_name] = value;
    }

    return env_vars;
}