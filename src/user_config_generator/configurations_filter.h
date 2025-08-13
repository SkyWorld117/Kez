#pragma once

#include <yaml-cpp/yaml.h>
#include <vector>
#include <string>

#include "environment_filter.h"
#include "options_filter.h"

YAML::Node filtered_configurations(const YAML::Node& config_node,
                                    const std::vector<std::string>& all_dependencies);
