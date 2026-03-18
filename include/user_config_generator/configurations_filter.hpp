#pragma once

#include <yaml-cpp/yaml.h>

#include <string>
#include <user_config_generator/environment_filter.hpp>
#include <user_config_generator/options_filter.hpp>
#include <vector>

YAML::Node filtered_configurations(const YAML::Node& config_node,
                                   const std::vector<std::string>& all_dependencies);
