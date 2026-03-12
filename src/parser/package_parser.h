#pragma once

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "../colors/colored_io.h"
#include "configuration_parser.h"
#include "scalar_parser.h"

std::vector<std::string> parse_package(const std::string& package_name,
                                       std::unordered_map<std::string, std::string>& template_map,
                                       const YAML::Node& user_config,
                                       const YAML::Node& user_config_pkg,
                                       const YAML::Node& user_config_context,
                                       const YAML::Node& pkg_config, const std::string& build_mode,
                                       const std::string& env_path);
