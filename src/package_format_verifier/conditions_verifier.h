#pragma once

#include <yaml-cpp/yaml.h>
#include <iostream>
#include <string>
#include <vector>
#include <cctype>

#include "../colors/colored_io.h"

bool verify_condition_format(std::string condition);
bool verify_conditions(const YAML::Node& node);