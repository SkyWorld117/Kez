// This file offers a function to resolve dependencies.

// It recursively reads the dependencies from a YAML file
// and returns a vector of ordered dependencies.

// This order should be considered as the build order.

// We assume the database is healthy.

#pragma once

#include <yaml-cpp/yaml.h>
#include <iostream>
#include <filesystem>
#include <vector>
#include <unordered_map>
#include <string>

#include "colors/colored_io.h"

std::vector<std::string> resolve_dependencies(const std::string& pkg_name);
std::vector<std::string> resolve_filtered_dependencies(const std::string& pkg_name);