#include "options_parser.h"

std::string parse_options(
    const YAML::Node& opts_node,
    std::unordered_map<std::string, std::string>& template_map,
    const YAML::Node& user_config,
    const YAML::Node& user_config_context,
    const YAML::Node& pkg_config,
    const std::string& build_mode,
    const std::string& env_path
) {
    std::string pkg_name = pkg_config["cheese"]["name"].as<std::string>();
    std::string options;

    for (const auto& opt : opts_node) {
        std::string opt_name = opt["name"].as<std::string>();
        std::string base_enabled, base_enabled_value, base_disabled_value;
        if (opt["user_configurable"] && opt["user_configurable"].as<bool>()) {
            YAML::Node user_config_opt;
            for (const auto& user_opt : user_config_context) {
                if (user_opt["name"].as<std::string>() == opt_name) {
                    user_config_opt = user_opt;
                    break;
                }
            }
            base_enabled = user_config_opt["enabled"].as<std::string>(); // Base enabled must exist in user_config
            if (user_config_opt["enabled_value"].IsNull()) {
                base_enabled_value = "";
            } else {
                base_enabled_value = user_config_opt["enabled_value"].as<std::string>();
            }
            if (user_config_opt["disabled_value"].IsNull()) {
                base_disabled_value = "";
            } else {
                base_disabled_value = user_config_opt["disabled_value"].as<std::string>();
            }
        } else {
            if (!opt["enabled"] || !opt["enabled"]["default"]) {
                base_enabled = "true"; // Default to true if not specified
            } else {
                base_enabled = opt["enabled"]["default"].as<std::string>();
            }
            if (!opt["enabled_value"] || opt["enabled_value"].IsNull()) {
                base_enabled_value = "";
            } else {
                base_enabled_value = opt["enabled_value"].as<std::string>();
            }
            if (!opt["disabled_value"] || opt["disabled_value"].IsNull()) {
                base_disabled_value = "";
            } else {
                base_disabled_value = opt["disabled_value"].as<std::string>();
            }
        }

        std::string final_enabled, final_enabled_value, final_disabled_value;

        if (opt["enabled"]["conditions"]) {
            final_enabled = parse_conditions(
                base_enabled,
                opt["enabled"],
                template_map,
                user_config,
                pkg_config,
                build_mode,
                env_path
            );
        } else {
            final_enabled = base_enabled;
        }

        if (opt["enabled_value"]["conditions"]) {
            final_enabled_value = parse_conditions(
                base_enabled_value,
                opt["enabled_value"],
                template_map,
                user_config,
                pkg_config,
                build_mode,
                env_path
            );
        } else {
            final_enabled_value = base_enabled_value;
        }
        if (!final_enabled_value.empty()) {
            final_enabled_value = parse_scalar(
                final_enabled_value,
                template_map,
                user_config,
                user_config_context,
                pkg_config,
                build_mode,
                env_path
            );
        }

        if (opt["disabled_value"]["conditions"]) {
            final_disabled_value = parse_conditions(
                base_disabled_value,
                opt["disabled_value"],
                template_map,
                user_config,
                pkg_config,
                build_mode,
                env_path
            );
        } else {
            final_disabled_value = base_disabled_value;
        }
        if (!final_disabled_value.empty()) {
            final_disabled_value = parse_scalar(
                final_disabled_value,
                template_map,
                user_config,
                user_config_context,
                pkg_config,
                build_mode,
                env_path
            );
        }

        if (final_enabled == "true") {
            std::string enabled_format = opt["enabled_format"] ? opt["enabled_format"].as<std::string>() : opt_name;
            options += enabled_format + (final_enabled_value.empty() ? "" : "=\"" + final_enabled_value + "\"");
            options += " ";
        } else if (final_enabled == "false") {
            std::string disabled_format = opt["disabled_format"] ? opt["disabled_format"].as<std::string>() : "";
            if (!disabled_format.empty()) {
               options += disabled_format + (final_disabled_value.empty() ? "" : "=\"" + final_disabled_value + "\"");
               options += " ";
            }
        } else {
            ERROR("Invalid enabled value for option '" + opt_name + "': " + final_enabled);
            exit(EXIT_FAILURE);
        }

        template_map[pkg_name + ".config." + opt_name] = final_enabled + "." + (final_enabled == "true" ? final_enabled_value : final_disabled_value);
    }

    return options;
}