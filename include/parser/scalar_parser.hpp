#pragma once

#include <yaml-cpp/yaml.h>

#include <parser/context.hpp>
#include <parser/template_parser.hpp>
#include <string>
#include <utils/colored_io.hpp>

std::string parse_scalar(const std::string& scalar_str, ParserContext& context);