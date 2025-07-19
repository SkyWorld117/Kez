#pragma once

#include <yaml-cpp/yaml.h>
#include <iostream>
#include <string>
#include <vector>

#include "../colors/colored_io.h"

bool verify_dependencies(const YAML::Node& node);