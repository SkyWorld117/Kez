#include <parser/options_parser.hpp>

std::string parse_options(const YAML::Node& opts_node, const std::string& toolchain,
                          ParserContext& context) {
    // Unpack context
    const std::string& package_name                            = context.package_name;
    const YAML::Node& user_config                              = context.user_config;
    const YAML::Node& user_config_context                      = context.user_config_context;
    const YAML::Node& pkg_config                               = context.pkg_config;
    std::unordered_map<std::string, std::string>& template_map = context.template_map;
    const std::string& env_path                                = context.env_path;

    std::string options;

    for (YAML::Node opt : opts_node) {
        std::string opt_name = opt["name"].as<std::string>();
        DEBUG("  - Parsing option: " + opt_name);
        std::string base_enabled, base_enabled_value, base_disabled_value;

        bool is_required = true;
        if (opt["requires"]) {
            std::vector<std::string> all_dependencies =
                user_config["recipe"]["dependencies"].as<std::vector<std::string>>();
            for (const auto& dep : opt["requires"]) {
                if (std::find(all_dependencies.begin(), all_dependencies.end(),
                              dep.as<std::string>()) == all_dependencies.end()) {
                    // Check if the required dependency is an abstract package and if any of its concrete implementations are present
                    std::string dep_name = dep.as<std::string>();
                    std::string dep_type =
                        get_db_config(dep_name)["cheese"]["type"].as<std::string>();
                    if (dep_type == "abstract") {
                        if (user_config["recipe"]["abstract_packages"][dep_name] &&
                            std::find(all_dependencies.begin(), all_dependencies.end(),
                                      user_config["recipe"]["abstract_packages"][dep_name]
                                          .as<std::string>()) != all_dependencies.end()) {
                            continue;
                        }
                    }
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
            final_enabled = parse_conditions(base_enabled, opt["enabled"]["conditions"], context);
        } else {
            final_enabled = base_enabled;
        }

        if (final_enabled == "true" && opt["enabled_value"]["conditions"]) {
            final_enabled_value =
                parse_conditions(base_enabled_value, opt["enabled_value"]["conditions"], context);
        } else {
            final_enabled_value = base_enabled_value;
        }
        if (final_enabled == "true" && !final_enabled_value.empty()) {
            final_enabled_value = parse_scalar(final_enabled_value, context);
        }

        if (final_enabled == "false" && opt["disabled_value"]["conditions"]) {
            final_disabled_value =
                parse_conditions(base_disabled_value, opt["disabled_value"]["conditions"], context);
        } else {
            final_disabled_value = base_disabled_value;
        }
        if (final_enabled == "false" && !final_disabled_value.empty()) {
            final_disabled_value = parse_scalar(final_disabled_value, context);
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

        template_map[package_name + ".config." + opt_name] =
            final_enabled + "." +
            (final_enabled == "true" ? final_enabled_value : final_disabled_value);
    }

    // Handle toolchain specific default options
    if (toolchain == "autotools") {
        if (options.find("--prefix") == std::string::npos) {
            options += "--prefix=" + env_path + " ";
        }
        template_map[package_name + ".config.--prefix"] = env_path;
    } else if (toolchain == "cmake") {
        if (options.find("-DCMAKE_PREFIX_PATH") == std::string::npos) {
            options += "-DCMAKE_PREFIX_PATH=" + env_path + " ";
        }
        template_map[package_name + ".config.-DCMAKE_PREFIX_PATH"] = env_path;
        if (options.find("-DCMAKE_BUILD_TYPE") == std::string::npos) {
            options += "-DCMAKE_BUILD_TYPE=Release ";
        }
        template_map[package_name + ".config.-DCMAKE_BUILD_TYPE"] = "Release";
    }

    // Remove trailing whitespace
    options.erase(options.find_last_not_of(" ") + 1);

    return options;
}
