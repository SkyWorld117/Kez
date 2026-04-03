#pragma once

#include <yaml-cpp/yaml.h>

#include <colors/colored_io.hpp>
#include <iostream>
#include <string>
#include <vector>

bool verify_release(const YAML::Node& node, const std::string& source_type);
bool verify_source(const YAML::Node& node);