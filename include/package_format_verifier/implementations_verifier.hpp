#pragma once

#include <yaml-cpp/yaml.h>

#include <utils/colored_io.hpp>
#include <iostream>
#include <string>
#include <vector>

bool verify_implementations(const YAML::Node& node);