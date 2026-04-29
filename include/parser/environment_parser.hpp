#pragma once

#include <yaml-cpp/yaml.h>

#include <parser/conditions_parser.hpp>
#include <parser/context.hpp>
#include <parser/scalar_parser.hpp>
#include <string>
#include <utils/colored_io.hpp>
#include <vector>

std::vector<std::string> parse_environment(const YAML::Node& env_node, ParserContext& context);
