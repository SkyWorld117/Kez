#pragma once

#include <yaml-cpp/yaml.h>

#include <utils/colored_io.hpp>
#include <database/database.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <parser/filter.hpp>
#include <parser/package_parser.hpp>
#include <parser/property_parser.hpp>
#include <string>
#include <unordered_map>
#include <vector>

YAML::Node parse(YAML::Node& config, const std::string& build_mode, const std::string& env_path);