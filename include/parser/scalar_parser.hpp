#pragma once

#include <yaml-cpp/yaml.h>

#include <colors/colored_io.hpp>
#include <iostream>
#include <parser/template_parser.hpp>
#include <string>
#include <unordered_map>
#include <vector>

std::string parse_scalar(const std::string& scalar_str,
                         std::unordered_map<std::string, std::string>& template_map,
                         const YAML::Node& user_config, const YAML::Node& user_config_pkg,
                         const std::string& build_mode, const std::string& env_path);