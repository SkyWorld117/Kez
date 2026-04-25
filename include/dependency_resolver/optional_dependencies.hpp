#pragma once

#include <yaml-cpp/yaml.h>

#include <utils/colored_io.hpp>
#include <database/database.hpp>
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

std::vector<std::string> get_optional_dependencies(const std::string& pkg_name);