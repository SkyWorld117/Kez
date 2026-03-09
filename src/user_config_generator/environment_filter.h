#pragma once

#include <yaml-cpp/yaml.h>

#include <string>
#include <vector>

#include "../colors/colored_io.h"

YAML::Node filtered_environment(const YAML::Node& env_node,
                                const std::vector<std::string>& all_dependencies);