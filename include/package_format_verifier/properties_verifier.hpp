#pragma once

#include <yaml-cpp/yaml.h>

#include <package_format_verifier/conditions_verifier.hpp>
#include <string>
#include <utils/colored_io.hpp>

bool verify_properties(const YAML::Node& node, const std::string& pkg_type);