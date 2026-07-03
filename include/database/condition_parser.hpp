#pragma once

#include <yaml-cpp/yaml.h>

#include <database/parser_context.hpp>
#include <string>

void validate_condition(const std::string& expression, const YAML::Node& node,
                        const std::string& path, const DatabaseParserContext& context);
void validate_templates(const YAML::Node& node, const std::string& path,
                        const DatabaseParserContext& context);
