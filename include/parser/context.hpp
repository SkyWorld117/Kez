#pragma once

#include <yaml-cpp/yaml.h>

#include <string>
#include <unordered_map>

struct ParserContext {
    std::string package_name;  // Name of the package being parsed
    std::unordered_map<std::string, std::string>
        template_map;                // For storing template variables and their values
    YAML::Node user_config;          // The entire user configuration
    YAML::Node user_config_pkg;      // Current package we are parsing
    YAML::Node user_config_context;  // For environment and options
    YAML::Node pkg_config;           // The package configuration from the database
    std::string build_mode;          // "debug" or "release"
    std::string env_path;            // Path to the installation environment
};