#pragma once

#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "../colors/colored_io.h"
#include "../database/database.h"
#include "filter.h"
#include "package_parser.h"
#include "property_parser.h"

YAML::Node parse(YAML::Node& config, const std::string& build_mode, const std::string& env_path);