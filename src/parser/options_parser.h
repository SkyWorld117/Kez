#pragma once

#include <yaml-cpp/yaml.h>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <string>

#include "../colors/colored_io.h"
#include "scalar_parser.h"
#include "conditions_parser.h"

std::string parse_options(
    const YAML::Node& opts_node,
    std::unordered_map<std::string, std::string>& template_map,
    const YAML::Node& user_config,
    const YAML::Node& user_config_context,
    const YAML::Node& pkg_config,
    const std::string& build_mode,
    const std::string& env_path
);