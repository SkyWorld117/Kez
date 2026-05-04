#pragma once

#include <yaml-cpp/yaml.h>

#include <functional>
#include <parser/context.hpp>
#include <parser/property_parser.hpp>
#include <string>
#include <utils/colored_io.hpp>

std::string parse_template(const std::string& template_str, ParserContext& context);

std::string resolve_templates_in_scalar(const std::string& input,
                                        std::function<std::string(const std::string&)> resolver,
                                        const std::string& error_context = "template");