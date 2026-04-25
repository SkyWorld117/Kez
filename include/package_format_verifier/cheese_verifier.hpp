#pragma once

#include <yaml-cpp/yaml.h>

#include <utils/colored_io.hpp>
#include <iostream>
#include <package_format_verifier/build_verifier.hpp>
#include <package_format_verifier/dependencies_verifier.hpp>
#include <package_format_verifier/implementations_verifier.hpp>
#include <package_format_verifier/properties_verifier.hpp>
#include <package_format_verifier/source_verifier.hpp>
#include <package_format_verifier/templates_verifier.hpp>
#include <string>
#include <vector>

bool verify_cheese(const YAML::Node& node);