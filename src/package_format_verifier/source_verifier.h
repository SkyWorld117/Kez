#pragma once

#include <yaml-cpp/yaml.h>
#include <iostream>
#include <string>
#include <vector>

#include "../colors/colored_io.h"

bool verify_release(const YAML::Node& node, const std::string& source_type);
bool verify_source(const YAML::Node& node);