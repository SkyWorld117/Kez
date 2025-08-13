#pragma once

#include <yaml-cpp/yaml.h>
#include <vector>
#include <string>

#include "../colors/colored_io.h"

YAML::Node filtered_environment(const YAML::Node& env_node,
                                const std::vector<std::string>& all_dependencies);