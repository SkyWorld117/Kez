#pragma once

#include <yaml-cpp/yaml.h>

#include <string>
#include <user_config_generator/configurations_filter.hpp>
#include <vector>

YAML::Node filtered_stages(const YAML::Node& stages_node,
                           const std::vector<std::string>& all_dependencies,
                           const YAML::Node& abstract_packages);