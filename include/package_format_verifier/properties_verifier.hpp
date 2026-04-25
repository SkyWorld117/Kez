#pragma once

#include <yaml-cpp/yaml.h>

#include <utils/colored_io.hpp>
#include <iostream>
#include <package_format_verifier/conditions_verifier.hpp>
#include <string>
#include <vector>

bool verify_properties(const YAML::Node& node, const std::string& pkg_type);