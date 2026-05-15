#pragma once

#include <yaml-cpp/yaml.h>

#include <string>
#include <utils/colored_io.hpp>
#include <vector>

YAML::Node filtered_environment(const YAML::Node& env_node,
                                const std::vector<std::string>& all_dependencies);