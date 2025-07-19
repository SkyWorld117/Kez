// This file offers a function to resolve dependencies.

// It recursively reads the dependencies from a YAML file
// and returns a vector of ordered dependencies.

// This order should be considered as the build order.

// We assume the database is healthy.

#ifndef deps_resolve

#define deps_resolve

#include <iostream>
#include <filesystem>
#include <yaml-cpp/yaml.h>

#include <vector>
#include <string>

#include <unordered_map>

std::vector<std::string> resolve_dependencies(const std::string& pkg_name);
std::vector<std::string> resolve_filtered_dependencies(const std::string& pkg_name);

#endif