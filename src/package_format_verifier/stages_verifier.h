#pragma once

#include <yaml-cpp/yaml.h>

#include <iostream>
#include <string>
#include <vector>

#include "../colors/colored_io.h"
#include "configurations_verifier.h"

bool verify_stages(const YAML::Node& node);