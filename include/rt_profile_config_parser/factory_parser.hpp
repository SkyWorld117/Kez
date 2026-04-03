#pragma once

#include <yaml-cpp/yaml.h>

#include <colors/colored_io.hpp>
#include <rt_profile_config_parser/cellar_parser.hpp>
#include <string>

YAML::Node parse_factory_config(const YAML::Node& factory_config);