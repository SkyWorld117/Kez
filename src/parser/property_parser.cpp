#include <filesystem>
#include <global_config.hpp>
#include <parser/property_parser.hpp>

std::string parse_property(const std::string& property_name, ParserContext& context) {
    // Unpack context
    std::unordered_map<std::string, std::string>& template_map = context.template_map;
    const YAML::Node user_config                               = context.user_config;
    const YAML::Node user_config_pkg                           = context.user_config_pkg;
    const std::string& build_mode                              = context.build_mode;
    const std::string& env_path                                = context.env_path;

    std::string package_name = property_name.substr(0, property_name.find('.'));
    std::string property     = property_name.substr(property_name.find('.') + 1);

    // Load the package configuration
    YAML::Node pkg_config_node = get_db_config(package_name);

    // Check if the package has the property
    if (property != "prefix" && property != "version" &&
        !pkg_config_node["cheese"]["properties"][property]) {
        ERROR("Property '" + property + "' not found in package '" + package_name + "'.");
        exit(EXIT_FAILURE);
    }

    std::string pkg_type = pkg_config_node["cheese"]["type"].as<std::string>();

    if (property == "prefix") {
        if (pkg_type == "system") {
            return global_config::get_path("system");
        } else if (pkg_type == "vendor" && pkg_config_node["cheese"]["properties"] &&
                   pkg_config_node["cheese"]["properties"]["prefix"]) {
            return parse_scalar(pkg_config_node["cheese"]["properties"]["prefix"].as<std::string>(),
                                context);
        }

        CellarPathQuery query;
        query.pkg_name = {package_name};
        if (pkg_type == "external") {
            query.pkg_version = global_config::get_external_package_version(package_name);
        } else {
            query.pkg_version = user_config["cheese"][package_name]["version"].as<std::string>();
        }
        if (user_config["cheese"][package_name]["compiler"]) {
            query.compiler_spec = user_config["cheese"][package_name]["compiler"].as<std::string>();
        }

        // Special packages
        if (pkg_type != "package") {
            return get_cellar_path(query);
        }

        // Regular packages
        if (build_mode == "release") {
            return env_path;
        } else if (build_mode == "debug") {
            // Get the first key in the user_config["cheese"] map
            std::string target_package = user_config["cheese"].begin()->first.as<std::string>();
            return env_path + "/" + package_name;
        } else {
            ERROR("Unknown build mode: " + build_mode);
            exit(EXIT_FAILURE);
        }
    } else if (property == "version") {
        if (pkg_type == "system") {
            std::filesystem::path state_path =
                std::filesystem::path(global_config::get_path("system")) / "state.yaml";
            YAML::Node state_node = YAML::LoadFile(state_path.string());
            if (state_node["cheese"] && state_node["cheese"][package_name] &&
                state_node["cheese"][package_name]["version"]) {
                return state_node["cheese"][package_name]["version"].as<std::string>();
            } else {
                ERROR("Package '" + package_name + "' version not found in state.yaml.");
                exit(EXIT_FAILURE);
            }
        } else if (pkg_type == "external") {
            return global_config::get_external_package_version(package_name);
        } else {
            if (user_config["cheese"][package_name]["version"]) {
                return user_config["cheese"][package_name]["version"].as<std::string>();
            } else {
                ERROR("Package '" + package_name + "' version not found in user config.");
                exit(EXIT_FAILURE);
            }
        }
    } else if (property == "c" or property == "cxx" or property == "fort" or
               property == "omp_flags") {
        std::string result = pkg_config_node["cheese"]["properties"][property].as<std::string>();
        std::string resolved_value  = parse_scalar(result, context);
        template_map[property_name] = resolved_value;
        return resolved_value;
    } else {
        // The rest of the properties needs to be solved in a second pass
        // due to possible dependencies on other properties
        return "${" + property_name + "}";
    }
}

std::string parse_complex_property(const std::string& template_str, ParserContext& context) {
    // Return the original template string if it doesn't contain a dot (i.e., not in the form of package.property)
    // This can likely happen when we are parsing a regular environment variable
    if (template_str.find('.') == std::string::npos) {
        return "${" + template_str + "}";
    }

    // Handle the special cases
    std::string package_name  = template_str.substr(0, template_str.find('.'));
    std::string property_name = template_str.substr(template_str.find('.') + 1);

    YAML::Node pkg_config_node = get_db_config(package_name);

    std::string base_value =
        pkg_config_node["cheese"]["properties"][property_name]["default"]
            ? pkg_config_node["cheese"]["properties"][property_name]["default"].as<std::string>()
            : "";
    std::string final_value;
    if (pkg_config_node["cheese"]["properties"][property_name]["conditions"]) {
        final_value = parse_conditions(
            base_value, pkg_config_node["cheese"]["properties"][property_name]["conditions"],
            context);
    } else {
        final_value = base_value;
    }
    if (!final_value.empty()) {
        // Resolve templates in the value
        // final_value = parse_scalar(final_value, template_map, user_config, user_config_pkg,
        //                            build_mode, env_path);
        final_value = parse_scalar(final_value, context);
    }
    context.template_map[template_str] = final_value;  // Cache the resolved template
    return final_value;
}

std::string parse_properties_in_scalar(const std::string& command, ParserContext& context) {
    auto resolver = [&](const std::string& property_name) {
        return parse_complex_property(property_name, context);
    };
    return resolve_templates_in_scalar(command, resolver, "property");
}
