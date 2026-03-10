#pragma once

#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <string>
#include <vector>

#include "../colors/colored_io.h"
#include "../database/database.h"
#include "../dependency_resolver/resolve_dependencies.h"
#include "configurations_filter.h"
#include "stages_filter.h"

YAML::Node gen_user_config(const std::string& pkg_name, bool interactive);