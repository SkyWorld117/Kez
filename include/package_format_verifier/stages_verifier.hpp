#pragma once

#include <yaml-cpp/yaml.h>

#include <utils/colored_io.hpp>
#include <iostream>
#include <package_format_verifier/configurations_verifier.hpp>
#include <string>
#include <vector>

bool verify_stages(const YAML::Node& node);