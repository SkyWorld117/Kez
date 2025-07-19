#pragma once

#include <yaml-cpp/yaml.h>
#include <iostream>
#include <string>
#include <vector>

#include "../colors/colored_io.h"
#include "conditions_verifier.h"

bool verify_properties(const YAML::Node& node, const std::string& pkg_type);