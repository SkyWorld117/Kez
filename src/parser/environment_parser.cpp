#include "environment_parser.h"

std::vector<std::string> parse_environment(
    const YAML::Node& env_node,
    std::unordered_map<std::string, std::string>& template_map,
    const YAML::Node& user_config,
    const YAML::Node& user_config_pkg,
    const YAML::Node& user_config_context,
    const YAML::Node& pkg_config,
    const std::string& build_mode,
    const std::string& env_path
) {
    std::string pkg_name = pkg_config["cheese"]["name"].as<std::string>();
    std::vector<std::string> env_vars;

    for (YAML::Node var : env_node) {
        std::string var_name = var["name"].as<std::string>();
        std::string base_value;
        if (var["user_configurable"] && var["user_configurable"].as<bool>()) {
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
            value = parse_conditions(
                base_value,
                var["conditions"],
                template_map,
                user_config,
                pkg_config,
                build_mode,
                env_path
            );
        } else {
            value = base_value;
        }

        if (!value.empty()) {
            // Resolve templates in the value
            value = parse_scalar(
                value,
                template_map,
                user_config,
                user_config_pkg,
                user_config_context,
                pkg_config,
                build_mode,
                env_path
            );
            env_vars.push_back(var_name + "=" + value);
        }

        template_map[pkg_name + ".env." + var_name] = value;
    }

    return env_vars;
}