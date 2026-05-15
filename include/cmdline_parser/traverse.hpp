#pragma once

#include <yaml-cpp/yaml.h>

#include <string>
#include <utils/colored_io.hpp>

void traverse(const std::string& path, const std::string& value, YAML::Node& node);