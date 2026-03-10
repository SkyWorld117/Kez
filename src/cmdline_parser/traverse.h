#pragma once

#include <yaml-cpp/yaml.h>

#include <string>

#include "../colors/colored_io.h"

void traverse(const std::string& path, const std::string& value, YAML::Node& node);