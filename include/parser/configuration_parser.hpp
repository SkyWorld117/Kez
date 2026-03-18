#pragma once

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <colors/colored_io.hpp>
#include <iostream>
#include <parser/environment_parser.hpp>
#include <parser/options_parser.hpp>
#include <string>
#include <unordered_map>
#include <vector>

// First string is the environment configuration
// Second string is the options configuration
std::pair<std::vector<std::string>, std::string> parse_configuration(
    YAML::Node& config, std::unordered_map<std::string, std::string>& template_map,
    const YAML::Node& user_config, const YAML::Node& user_config_pkg,
    const YAML::Node& user_config_context, const YAML::Node& pkg_config,
    const std::string& build_mode, const std::string& env_path);