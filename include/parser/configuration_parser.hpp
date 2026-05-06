#pragma once

#include <yaml-cpp/yaml.h>

#include <parser/context.hpp>
#include <parser/environment_parser.hpp>
#include <parser/options_parser.hpp>
#include <string>
#include <utils/colored_io.hpp>
#include <vector>

// First string is the environment configuration
// Second string is the options configuration
void parse_configuration(std::vector<std::string>& instructions,
                         const std::string& command,
                         YAML::Node& config,
                         const std::string& toolchain,
                         ParserContext& context);