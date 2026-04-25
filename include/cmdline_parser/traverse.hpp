#pragma once

#include <yaml-cpp/yaml.h>

#include <utils/colored_io.hpp>
#include <string>

void traverse(const std::string& path, const std::string& value, YAML::Node& node);