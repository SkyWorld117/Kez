#pragma once

#include <yaml-cpp/yaml.h>

#include <colors/colored_io.hpp>
#include <iostream>
#include <package_format_verifier/conditions_verifier.hpp>
#include <string>
#include <vector>

bool verify_options(const YAML::Node& node);