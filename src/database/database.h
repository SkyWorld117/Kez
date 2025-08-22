#pragma once

#include <yaml-cpp/yaml.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>

#include "../colors/colored_io.h"

YAML::Node get_db_config(std::string pkg_name);