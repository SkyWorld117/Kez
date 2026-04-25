#pragma once

#include <yaml-cpp/yaml.h>

#include <utils/colored_io.hpp>
#include <iostream>
#include <package_format_verifier/environment_verifier.hpp>
#include <package_format_verifier/options_verifier.hpp>
#include <string>
#include <vector>

bool verify_configurations(const YAML::Node& node);