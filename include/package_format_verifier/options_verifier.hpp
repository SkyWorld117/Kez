#pragma once

#include <yaml-cpp/yaml.h>

#include <package_format_verifier/conditions_verifier.hpp>
#include <utils/colored_io.hpp>

bool verify_options(const YAML::Node& node);