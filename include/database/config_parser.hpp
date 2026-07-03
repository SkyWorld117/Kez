#pragma once

#include <yaml-cpp/yaml.h>

#include <database/database.hpp>
#include <database/parser_context.hpp>

PackageConfigPtr parse_config_document(const YAML::Node& document,
                                       const DatabaseParserContext& context);
