#pragma once

#include <yaml-cpp/yaml.h>

#include <parser/configuration_parser.hpp>
#include <parser/context.hpp>
#include <parser/scalar_parser.hpp>
#include <string>
#include <utils/colored_io.hpp>
#include <vector>

std::vector<std::string> parse_package(ParserContext& context);
