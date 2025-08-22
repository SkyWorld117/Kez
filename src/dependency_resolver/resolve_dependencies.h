#pragma once

#include <yaml-cpp/yaml.h>
#include <iostream>
#include <filesystem>
#include <vector>
#include <unordered_map>
#include <string>
#include <algorithm>

#include "../colors/colored_io.h"
#include "essential_dependencies.h"
#include "optional_dependencies.h"
#include "toposort.h"
#include "advisor.h"
#include "../database/database.h"

std::pair<std::pair<std::vector<std::string>, std::vector<std::string>>, std::unordered_map<std::string, std::string>> resolve_dependencies(const std::string& pkg_name, bool interactive);