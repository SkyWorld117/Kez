#pragma once

#include <yaml-cpp/yaml.h>

#include <iostream>
#include <string>
#include <vector>

#include "../colors/colored_io.h"
#include "configurations_verifier.h"
#include "stages_verifier.h"

bool verify_build(const YAML::Node& node);