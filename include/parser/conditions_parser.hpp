#pragma once

#include <yaml-cpp/yaml.h>

#include <database/database.hpp>
#include <parser/context.hpp>
#include <string>
#include <utils/colored_io.hpp>

std::string parse_conditions(const std::string& base_value, const YAML::Node& conditions_node,
                             ParserContext& context);