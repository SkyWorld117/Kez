#pragma once

#include <yaml-cpp/yaml.h>

#include <package_format_verifier/environment_verifier.hpp>
#include <package_format_verifier/options_verifier.hpp>
#include <utils/colored_io.hpp>

bool verify_configurations(const YAML::Node& node);