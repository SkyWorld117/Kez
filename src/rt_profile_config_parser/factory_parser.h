#pragma once

#include <yaml-cpp/yaml.h>

#include <string>

#include "../colors/colored_io.h"
#include "cellar_parser.h"

YAML::Node parse_factory_config(const YAML::Node& factory_config);