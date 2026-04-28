#pragma once

#include <yaml-cpp/yaml.h>

#include <database/database.hpp>
#include <parser/filter.hpp>
#include <parser/package_parser.hpp>
#include <parser/property_parser.hpp>
#include <string>
#include <utils/colored_io.hpp>

YAML::Node parse(YAML::Node& config, const std::string& build_mode, const std::string& env_path);