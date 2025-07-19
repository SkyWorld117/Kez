#pragma once

#include <yaml-cpp/yaml.h>
#include <iostream>
#include <string>
#include <vector>
#include <cctype>

#include "../colors/colored_io.h"

bool verify_templates(const YAML::Node& node);