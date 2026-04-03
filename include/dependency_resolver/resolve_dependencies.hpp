#pragma once

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <colors/colored_io.hpp>
#include <database/database.hpp>
#include <dependency_resolver/advisor.hpp>
#include <dependency_resolver/essential_dependencies.hpp>
#include <dependency_resolver/optional_dependencies.hpp>
#include <dependency_resolver/toposort.hpp>
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

std::pair<std::pair<std::vector<std::string>, std::vector<std::string>>,
          std::unordered_map<std::string, std::string>>
    resolve_dependencies(const std::string& pkg_name, bool interactive);