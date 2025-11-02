#pragma once

#include <yaml-cpp/yaml.h>
#include <string>
#include <vector>

#include "../colors/colored_io.h"
#include "resource_manager.h"

YAML::Node parse_run_config(const YAML::Node& factory_config, const YAML::Node& cellar_config, const YAML::Node& profile_config);