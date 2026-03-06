#pragma once

#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "../colors/colored_io.h"
#include "../database/database.h"

std::vector<std::string> get_essential_dependencies(const std::string& pkg_name);