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
        template_map[property_name] = result;
        return result;
    } else {
        // For other properties, we need to parse the conditions
        // std::string base_value = pkg_config_node["cheese"]["properties"][property]["default"] ? 
        //     pkg_config_node["cheese"]["properties"][property]["default"].as<std::string>() : "";

        // Return the property string directly for now
        return "${" + property_name + "}";
    }
}