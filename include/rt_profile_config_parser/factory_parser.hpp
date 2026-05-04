#pragma once

#include <yaml-cpp/yaml.h>

#include <rt_profile_config_parser/cellar_parser.hpp>
#include <utils/colored_io.hpp>

YAML::Node parse_factory_config(const YAML::Node& factory_config);