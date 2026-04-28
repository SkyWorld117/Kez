#pragma once

#include <yaml-cpp/yaml.h>

#include <package_format_verifier/configurations_verifier.hpp>
#include <utils/colored_io.hpp>

bool verify_stages(const YAML::Node& node);