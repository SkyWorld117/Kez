#pragma once

#include <yaml-cpp/yaml.h>

#include <string>
#include <utils/colored_io.hpp>

bool verify_release(const YAML::Node& node, const std::string& source_type);
bool verify_source(const YAML::Node& node);