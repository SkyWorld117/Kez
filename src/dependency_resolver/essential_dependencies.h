#pragma once

#include <yaml-cpp/yaml.h>
#include <iostream>
#include <filesystem>
#include <vector>
#include <unordered_map>
#include <string>

#include "../colors/colored_io.h"

std::vector<std::string> get_essential_dependencies(const std::string& pkg_name);