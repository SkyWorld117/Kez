#pragma once

#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "../colors/colored_io.h"
#include "property_parser.h"

std::string parse_template(const std::string&                            template_str,
                           std::unordered_map<std::string, std::string>& template_map,
                           const YAML::Node& user_config, const YAML::Node& user_config_pkg,
                           const std::string& build_mode, const std::string& env_path);