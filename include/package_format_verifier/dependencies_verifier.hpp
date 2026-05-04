#pragma once

#include <yaml-cpp/yaml.h>

#include <utils/colored_io.hpp>

bool verify_dependencies(const YAML::Node& node);