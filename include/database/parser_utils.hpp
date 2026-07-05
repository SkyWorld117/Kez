#pragma once

#include <yaml-cpp/yaml.h>

#include <database/config.hpp>
#include <database/parser_context.hpp>
#include <initializer_list>
#include <optional>
#include <string>

[[noreturn]] void fail_config(const YAML::Node& node, const std::string& yaml_path,
                              const std::string& message, const DatabaseParserContext& context);
void warn_config(const YAML::Node& node, const std::string& yaml_path, const std::string& message,
                 const DatabaseParserContext& context);

void expect_map(const YAML::Node& node, const std::string& path,
                const DatabaseParserContext& context);
void expect_sequence(const YAML::Node& node, const std::string& path,
                     const DatabaseParserContext& context);
void check_keys(const YAML::Node& node, std::initializer_list<const char*> allowed_keys,
                const std::string& path, const DatabaseParserContext& context);
YAML::Node required_node(const YAML::Node& map, const std::string& key, const std::string& path,
                         const DatabaseParserContext& context);
std::string parse_scalar(const YAML::Node& node, const std::string& path,
                         const DatabaseParserContext& context, bool allow_null = false);
std::string required_scalar(const YAML::Node& map, const std::string& key, const std::string& path,
                            const DatabaseParserContext& context);
std::optional<std::string> optional_scalar(const YAML::Node& map, const std::string& key,
                                           const std::string& path,
                                           const DatabaseParserContext& context);
bool parse_boolean(const YAML::Node& node, const std::string& path,
                   const DatabaseParserContext& context);
ValueAction parse_action(const YAML::Node& node, const std::string& path,
                         const DatabaseParserContext& context);
