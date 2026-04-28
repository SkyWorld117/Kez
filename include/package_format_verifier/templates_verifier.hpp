#pragma once

#include <yaml-cpp/yaml.h>

#include <cctype>
#include <utils/colored_io.hpp>

bool verify_templates(const YAML::Node& node);