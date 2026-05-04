#pragma once

#include <yaml-cpp/yaml.h>

#include <database/database.hpp>
#include <string>
#include <utils/colored_io.hpp>
#include <vector>

std::vector<std::string> get_optional_dependencies(const std::string& pkg_name);