#pragma once

#include <yaml-cpp/yaml.h>

#include <utils/colored_io.hpp>
#include <iostream>
#include <string>
#include <vector>

bool verify_dependencies(const YAML::Node& node);