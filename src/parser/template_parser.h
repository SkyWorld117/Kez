#pragma once

#include <yaml-cpp/yaml.h>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <string>

#include "../colors/colored_io.h"

std::string parse_template(
    const std::string& template_str,
    std::unordered_map<std::string, std::string>& template_map,
    const YAML::Node& user_config,
    const YAML::Node& user_config_pkg_context,
    const YAML::Node& pkg_config,
    const std::string& build_mode,
    const std::string& env_path
);