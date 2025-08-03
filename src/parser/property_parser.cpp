#include "property_parser.h"

std::string parse_property(
    const std::string& property_name,
    std::unordered_map<std::string, std::string>& template_map,
    const YAML::Node& user_config,
    const YAML::Node& user_config_pkg,
    const YAML::Node& user_config_context,
    const YAML::Node& pkg_config,
    const std::string& build_mode,
    const std::string& env_path
) {
    std::string package_name = property_name.substr(0, property_name.find('.'));
    std::string property = property_name.substr(property_name.find('.') + 1);

    // Load the package configuration
    std::filesystem::path db_path(getenv("CHEESE_DB"));
    std::filesystem::path package_config_path = db_path / (package_name + ".yaml");

    YAML::Node pkg_config_node = YAML::LoadFile(package_config_path.string());

    // Check if the package has the property
    if (property != "prefix" && property != "version" &&
        !pkg_config_node["cheese"]["properties"][property]) {
        ERROR("Property '" + property + "' not found in package '" + package_name + "'.");
        exit(EXIT_FAILURE);
    }

    if (property == "prefix") {
        if (build_mode == "release") {
            return env_path;
        } else if (build_mode == "debug") {
            // Get the first key in the user_config["cheese"] map
            std::string target_package = user_config["cheese"].begin()->first.as<std::string>();
            std::string current_package = pkg_config["cheese"]["name"].as<std::string>();
            if (target_package == current_package) {
                return env_path + "/" + target_package;
            } else {
                return env_path + "/deps";
            }
        } else {
            ERROR("Unknown build mode: " + build_mode);
            exit(EXIT_FAILURE);
        }
    } else if (property == "version") {
        // TODO: Implement version fetching logic
        return "x.x.x";
    } else if (property == "c" or
               property == "cxx" or
               property == "fort" or
               property == "omp_flags") {
        std::string result = pkg_config_node["cheese"]["properties"][property].as<std::string>();
        std::string resolved_value = parse_scalar(
            result,
            template_map,
            user_config,
            user_config_pkg,
            user_config_context,
            pkg_config_node,
            build_mode,
            env_path
        );
        template_map[property_name] = resolved_value;
        return resolved_value;
    } else {
        // The rest of the properties needs to be solved in a second pass
        // due to possible dependencies on other properties
        return "${" + property_name + "}";
    }
}

std::string parse_complex_property(
    const std::string& template_str,
    std::unordered_map<std::string, std::string>& template_map,
    const YAML::Node& user_config,
    const YAML::Node& user_config_pkg,
    const std::string& build_mode,
    const std::string& env_path
) {
    // Handle the special cases
    std::string package_name = template_str.substr(0, template_str.find('.'));
    std::string property_name = template_str.substr(template_str.find('.') + 1);

    std::filesystem::path db_path(getenv("CHEESE_DB"));
    std::filesystem::path package_config_path = db_path / (package_name + ".yaml");
    // No need to perform a check again, as it is already done in the first pass
    YAML::Node pkg_config_node = YAML::LoadFile(package_config_path.string());

    std::string base_value = pkg_config_node["cheese"]["properties"][property_name]["default"] ?
        pkg_config_node["cheese"]["properties"][property_name]["default"].as<std::string>() : "";
    std::string final_value;
    if (pkg_config_node["cheese"]["properties"][property_name]["conditions"]) {
        final_value = parse_conditions(
            base_value,
            pkg_config_node["cheese"]["properties"][property_name]["conditions"],
            template_map,
            user_config,
            pkg_config_node,
            build_mode,
            env_path
        );
    } else {
        final_value = base_value;
    }
    if (!final_value.empty()) {
        // Resolve templates in the value
        final_value = parse_scalar(
            final_value,
            template_map,
            user_config,
            user_config_pkg,
            user_config_pkg, // Use pkg_config as context
            pkg_config_node,
            build_mode,
            env_path
        );
    }
    template_map[template_str] = final_value; // Cache the resolved template
    return final_value;
}

std::string parse_properties_in_scalar(
    const std::string& command,
    std::unordered_map<std::string, std::string>& template_map,
    const YAML::Node& user_config,
    const YAML::Node& user_config_pkg,
    const std::string& build_mode,
    const std::string& env_path
) {
    std::string result = command;
    size_t pos = 0;
    while ((pos = result.find("${", pos)) != std::string::npos) {
        size_t end_pos = result.find('}', pos);
        if (end_pos == std::string::npos) {
            ERROR("Unclosed property template in string: " + result);
            exit(EXIT_FAILURE);
        }
        std::string property_name = result.substr(pos + 2, end_pos - pos - 2);
        std::string resolved_property = parse_complex_property(
            property_name,
            template_map,
            user_config,
            user_config_pkg,
            build_mode,
            env_path
        );
        result.replace(pos, end_pos - pos + 1, resolved_property);
        pos += resolved_property.length(); // Move past the resolved property
    }

    return result;
}