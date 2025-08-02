#pragma once

#include <yaml-cpp/yaml.h>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <string>

#include "../colors/colored_io.h"

std::string parse_conditions(
    const std::string& base_value,
    const YAML::Node& conditions_node,
    const std::unordered_map<std::string, std::string>& template_map,
    const YAML::Node& user_config,
    const YAML::Node& pkg_config,
    const std::string& build_mode,
    const std::string& env_path
);