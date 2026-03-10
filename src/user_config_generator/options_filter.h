#pragma once

#include <yaml-cpp/yaml.h>

#include <string>
#include <vector>

#include "../colors/colored_io.h"

YAML::Node filtered_options(const YAML::Node& options_node,
                            const std::vector<std::string>& all_dependencies);