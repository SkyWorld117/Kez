#pragma once

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "../colors/colored_io.h"
#include "../database/database.h"
#include "advisor.h"
#include "essential_dependencies.h"
#include "optional_dependencies.h"
#include "toposort.h"

std::pair<std::pair<std::vector<std::string>, std::vector<std::string>>,
          std::unordered_map<std::string, std::string>>
    resolve_dependencies(const std::string& pkg_name, bool interactive);