#pragma once

#include <yaml-cpp/yaml.h>

#include <database/database.hpp>
#include <parser/conditions_parser.hpp>
#include <parser/scalar_parser.hpp>
#include <string>
#include <utils/colored_io.hpp>

std::string parse_fromager_template(const std::string& command);
std::string parse_fromager_arch(const std::string& template_str);