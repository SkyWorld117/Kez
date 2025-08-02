#pragma once

#include <yaml-cpp/yaml.h>
#include <iostream>
#include <filesystem>
#include <vector>
#include <unordered_map>
#include <string>

#include "../colors/colored_io.h"

std::vector<std::string> get_templates(const YAML::Node& config);
std::unordered_map<std::string, std::string> broadcast(const YAML::Node& config);