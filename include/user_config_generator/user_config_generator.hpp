#pragma once

#include <yaml-cpp/yaml.h>

#include <database/database.hpp>
#include <dependency_resolver/resolve_dependencies.hpp>
#include <global_config.hpp>
#include <string>
#include <user_config_generator/configurations_filter.hpp>
#include <user_config_generator/stages_filter.hpp>
#include <utils/colored_io.hpp>
#include <vector>

YAML::Node gen_user_config(const std::vector<std::string>& pkg_name, bool interactive);