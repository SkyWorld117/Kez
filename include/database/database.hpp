#pragma once

#include <yaml-cpp/yaml.h>

#include <string>
#include <utils/colored_io.hpp>

YAML::Node get_db_config(const std::string& pkg_name);