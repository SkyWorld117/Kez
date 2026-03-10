#pragma once

#include <yaml-cpp/yaml.h>

#include <cctype>
#include <iostream>
#include <string>
#include <vector>

#include "../colors/colored_io.h"

bool verify_templates(const YAML::Node& node);