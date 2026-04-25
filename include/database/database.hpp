#pragma once

#include <yaml-cpp/yaml.h>

#include <utils/colored_io.hpp>
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

YAML::Node get_db_config(std::string pkg_name);