#pragma once

#include <yaml-cpp/yaml.h>
#include <string>

#include "../colors/colored_io.h"
#include "run_config_parser.h"

YAML::Node parse_cellar_config(const YAML::Node& factory_config, const YAML::Node& cellar_config);