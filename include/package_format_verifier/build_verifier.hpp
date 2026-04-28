#pragma once

#include <yaml-cpp/yaml.h>

#include <package_format_verifier/configurations_verifier.hpp>
#include <package_format_verifier/stages_verifier.hpp>
#include <utils/colored_io.hpp>

bool verify_build(const YAML::Node& node);