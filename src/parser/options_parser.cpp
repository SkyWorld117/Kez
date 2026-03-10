#include "options_parser.h"

std::string parse_options(const YAML::Node &opts_node,
                          std::unordered_map<std::string, std::string> &template_map,
                          const YAML::Node &user_config, const YAML::Node &user_config_pkg,
                          const YAML::Node &user_config_context, const YAML::Node &pkg_config,
                          const std::string &build_mode, const std::string &env_path) {
    std::string pkg_name = pkg_config["cheese"]["name"].as<std::string>();
    std::string options;

    for (YAML::Node opt : opts_node) {
        std::string opt_name = opt["name"].as<std::string>();
        if (build_mode == "debug") {
            INFO("  - Parsing option: " + opt_name);
        }
        std::string base_enabled, base_enabled_value, base_disabled_value;

        bool is_required = true;
        if (opt["requires"]) {
            std::vector<std::string> all_dependencies =
                user_config["recipe"]["dependencies"].as<std::vector<std::string>>();
            for (const auto &dep : opt["requires"]) {
                if (std::find(all_dependencies.begin(), all_dependencies.end(),
                              dep.as<std::string>()) == all_dependencies.end()) {
                    is_required = false;
                    break;
                }
            }
        }
        if (opt["user_configurable"] && opt["user_configurable"].as<bool>() && is_required) {
            YAML::Node user_config_opt;
            for (YAML::Node user_opt : user_config_context) {
                if (user_opt["name"].as<std::string>() == opt_name) {
                    user_config_opt = user_opt;
                    break;
                }
            }
            if (!user_config_opt["enabled"]) {
                if (!opt["enabled"]["conditions"]) {
                    ERROR("Option '" + opt_name +
                          "' is user configurable but 'enabled' field is missing in user "
                          "configuration.");
                    exit(EXIT_FAILURE);
                }
                base_enabled = "false";
            } else {
                base_enabled = user_config_opt["enabled"].as<std::string>();
            }
            if (user_config_opt.IsNull() || !user_config_opt["enabled_value"] ||
                user_config_opt["enabled_value"].IsNull()) {
                base_enabled_value = "";
            } else {
                base_enabled_value = user_config_opt["enabled_value"].as<std::string>();
            }
            if (user_config_opt.IsNull() || !user_config_opt["disabled_value"] ||
                user_config_opt["disabled_value"].IsNull()) {
                base_disabled_value = "";
            } else {
                base_disabled_value = user_config_opt["disabled_value"].as<std::string>();
            }
        } else {
            if (!opt["enabled"] || !opt["enabled"]["default"]) {
                base_enabled = "true";  // Default to true if not specified
            } else {
                base_enabled = opt["enabled"]["default"].as<std::string>();
            }
            if (!opt["enabled_value"] || !opt["enabled_value"]["default"]) {
                base_enabled_value = "";
            } else {
                base_enabled_value = opt["enabled_value"]["default"].as<std::string>();
            }
            if (!opt["disabled_value"] || !opt["disabled_value"]["default"]) {
                base_disabled_value = "";
            } else {
                base_disabled_value = opt["disabled_value"]["default"].as<std::string>();
            }
        }

        std::string final_enabled, final_enabled_value, final_disabled_value;

        if (opt["enabled"]["conditions"]) {
            final_enabled =
                parse_conditions(base_enabled, opt["enabled"]["conditions"], template_map,
                                 user_config, pkg_config, build_mode, env_path);
        } else {
            final_enabled = base_enabled;
        }

        if (final_enabled == "true" && opt["enabled_value"]["conditions"]) {
            final_enabled_value =
                parse_conditions(base_enabled_value, opt["enabled_value"]["conditions"],
                                 template_map, user_config, pkg_config, build_mode, env_path);
        } else {
            final_enabled_value = base_enabled_value;
        }
        if (final_enabled == "true" && !final_enabled_value.empty()) {
            final_enabled_value = parse_scalar(final_enabled_value, template_map, user_config,
                                               user_config_pkg, build_mode, env_path);
        }

        if (final_enabled == "false" && opt["disabled_value"]["conditions"]) {
            final_disabled_value =
                parse_conditions(base_disabled_value, opt["disabled_value"]["conditions"],
                                 template_map, user_config, pkg_config, build_mode, env_path);
        } else {
            final_disabled_value = base_disabled_value;
        }
        if (final_enabled == "false" && !final_disabled_value.empty()) {
            final_disabled_value = parse_scalar(final_disabled_value, template_map, user_config,
                                                user_config_pkg, build_mode, env_path);
        }

        if (final_enabled == "true") {
            std::string enabled_format =
                opt["enabled_format"] ? opt["enabled_format"].as<std::string>() : opt_name;
            // This happens only if a developer explicitly set `enabled_format` to an
            // empty string It is useful if one only wishes to store the value for
            // parsing
            if (!enabled_format.empty()) {
                options += enabled_format +
                           (final_enabled_value.empty() ? "" : "=\"" + final_enabled_value + "\"");
                options += " ";
            }
        } else if (final_enabled == "false") {
            std::string disabled_format =
                opt["disabled_format"] ? opt["disabled_format"].as<std::string>() : "";
            if (!disabled_format.empty()) {
                options +=
                    disabled_format +
                    (final_disabled_value.empty() ? "" : "=\"" + final_disabled_value + "\"");
                options += " ";
            }
        } else {
            ERROR("Invalid enabled value for option '" + opt_name + "': " + final_enabled);
            exit(EXIT_FAILURE);
        }

        template_map[pkg_name + ".config." + opt_name] =
            final_enabled + "." +
            (final_enabled == "true" ? final_enabled_value : final_disabled_value);
    }

    // Remove trailing whitespace
    options.erase(options.find_last_not_of(" ") + 1);

    return options;
}
