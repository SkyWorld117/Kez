#pragma once

#include <yaml-cpp/yaml.h>

#include <parser/conditions_parser.hpp>
#include <parser/context.hpp>
#include <parser/scalar_parser.hpp>
#include <string>
#include <utils/colored_io.hpp>

std::string parse_options(const YAML::Node &opts_node, const std::string &toolchain,
                          ParserContext &context);