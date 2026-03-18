#pragma once

#include <yaml-cpp/yaml.h>

#include <colors/colored_io.hpp>
#include <iostream>
#include <package_format_verifier/configurations_verifier.hpp>
#include <package_format_verifier/stages_verifier.hpp>
#include <string>
#include <vector>

bool verify_build(const YAML::Node& node);