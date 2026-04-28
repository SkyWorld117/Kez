#pragma once

#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utils/colored_io.hpp>
#include <vector>

YAML::Node get_db_config(const std::string& pkg_name);