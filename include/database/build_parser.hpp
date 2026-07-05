#pragma once

#include <yaml-cpp/yaml.h>

#include <database/config.hpp>
#include <database/parser_context.hpp>
#include <string>

ConfigurableValue<bool> parse_bool_configurable(const YAML::Node& node, const std::string& path,
                                                const DatabaseParserContext& context);
ConfigurableValue<std::string> parse_string_configurable(const YAML::Node& node,
                                                         const std::string& path,
                                                         const DatabaseParserContext& context);
BuildConfiguration parse_build_configuration(const YAML::Node& node, const std::string& path,
                                             const DatabaseParserContext& context);
Build parse_build(const YAML::Node& node, const std::string& path,
                  const DatabaseParserContext& context);
