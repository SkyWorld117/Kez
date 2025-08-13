#pragma once

#include <yaml-cpp/yaml.h>
#include <vector>
#include <string>

#include "configurations_filter.h"

YAML::Node filtered_stages(const YAML::Node& stages_node,
                           const std::vector<std::string>& all_dependencies);