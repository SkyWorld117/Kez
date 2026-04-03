#pragma once

#include <yaml-cpp/yaml.h>

#include <colors/colored_io.hpp>
#include <database/database.hpp>
#include <string>
#include <vector>

YAML::Node filtered_options(const YAML::Node& options_node,
                            const std::vector<std::string>& all_dependencies,
                            const YAML::Node& abstract_packages);