#pragma once

#include <yaml-cpp/yaml.h>
#include <iostream>
#include <filesystem>
#include <vector>
#include <unordered_map>
#include <string>

#include "../colors/colored_io.h"
#include "essential_dependencies.h"
#include "optional_dependencies.h"
#include "toposort.h"

std::vector<std::string> resolve_dependencies(const std::string& pkg_name);