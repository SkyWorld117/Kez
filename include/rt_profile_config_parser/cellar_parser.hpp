#pragma once

#include <yaml-cpp/yaml.h>

#include <colors/colored_io.hpp>
#include <rt_profile_config_parser/run_config_parser.hpp>
#include <string>

YAML::Node parse_cellar_config(const YAML::Node& factory_config, const YAML::Node& cellar_config);