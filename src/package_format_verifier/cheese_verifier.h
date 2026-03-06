#pragma once

#include <yaml-cpp/yaml.h>

#include <iostream>
#include <string>
#include <vector>

#include "../colors/colored_io.h"
#include "build_verifier.h"
#include "dependencies_verifier.h"
#include "implementations_verifier.h"
#include "properties_verifier.h"
#include "source_verifier.h"
#include "templates_verifier.h"

bool verify_cheese(const YAML::Node& node);