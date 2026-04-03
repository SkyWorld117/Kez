#pragma once

#include <yaml-cpp/yaml.h>

#include <cctype>
#include <colors/colored_io.hpp>
#include <iostream>
#include <string>
#include <vector>

bool verify_templates(const YAML::Node& node);