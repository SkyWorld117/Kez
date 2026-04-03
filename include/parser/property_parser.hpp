#pragma once

#include <yaml-cpp/yaml.h>

#include <colors/colored_io.hpp>
#include <database/database.hpp>
#include <filesystem>
#include <iostream>
#include <parser/conditions_parser.hpp>
#include <parser/scalar_parser.hpp>
#include <string>
#include <unordered_map>
#include <vector>

std::string parse_property(const std::string& property_name,
                           std::unordered_map<std::string, std::string>& template_map,
                           const YAML::Node& user_config, const YAML::Node& user_config_pkg,
                           const std::string& build_mode, const std::string& env_path);

std::string parse_properties_in_scalar(const std::string& command,
                                       std::unordered_map<std::string, std::string>& template_map,
                                       const YAML::Node& user_config,
                                       const YAML::Node& user_config_pkg,
                                       const std::string& build_mode, const std::string& env_path);