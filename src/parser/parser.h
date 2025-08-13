#pragma once

#include <yaml-cpp/yaml.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <unordered_map>
#include <string>

#include "../colors/colored_io.h"

#include "package_parser.h"
#include "property_parser.h"
#include "filter.h"

YAML::Node parse(YAML::Node& config, const std::string& build_mode, const std::string& env_path);