#pragma once

#include <yaml-cpp/yaml.h>
#include <vector>
#include <string>

#include "../colors/colored_io.h"

YAML::Node filtered_options(const YAML::Node& options_node, 
                            const std::vector<std::string>& all_dependencies);