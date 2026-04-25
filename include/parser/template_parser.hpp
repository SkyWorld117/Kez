#pragma once

#include <yaml-cpp/yaml.h>

#include <utils/colored_io.hpp>
#include <filesystem>
#include <iostream>
#include <parser/property_parser.hpp>
#include <string>
#include <unordered_map>
#include <vector>

std::string parse_template(const std::string& template_str,
                           std::unordered_map<std::string, std::string>& template_map,
                           const YAML::Node& user_config, const YAML::Node& user_config_pkg,
                           const std::string& build_mode, const std::string& env_path);