#pragma once

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <colors/colored_io.hpp>
#include <iostream>
#include <parser/configuration_parser.hpp>
#include <parser/scalar_parser.hpp>
#include <string>
#include <unordered_map>
#include <vector>

std::vector<std::string> parse_package(const std::string& package_name,
                                       std::unordered_map<std::string, std::string>& template_map,
                                       const YAML::Node& user_config,
                                       const YAML::Node& user_config_pkg,
                                       const YAML::Node& user_config_context,
                                       const YAML::Node& pkg_config, const std::string& build_mode,
                                       const std::string& env_path);
