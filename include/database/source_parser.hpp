#pragma once

#include <yaml-cpp/yaml.h>

#include <database/config.hpp>
#include <database/parser_context.hpp>
#include <string>
#include <vector>

Source parse_source(const YAML::Node& node, const std::string& path,
                    const DatabaseParserContext& context);
std::vector<std::string> parse_scalar_sequence(const YAML::Node& node, const std::string& path,
                                               const DatabaseParserContext& context);
