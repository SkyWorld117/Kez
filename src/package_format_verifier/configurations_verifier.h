#pragma once

#include <yaml-cpp/yaml.h>

#include <iostream>
#include <string>
#include <vector>

#include "../colors/colored_io.h"
#include "environment_verifier.h"
#include "options_verifier.h"

bool verify_configurations(const YAML::Node& node);