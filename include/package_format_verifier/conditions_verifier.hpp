#pragma once

#include <yaml-cpp/yaml.h>

#include <cctype>
#include <string>
#include <utils/colored_io.hpp>

bool verify_condition_format(std::string condition);
bool verify_conditions(const YAML::Node& node);