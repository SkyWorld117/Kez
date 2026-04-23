#pragma once

#include <yaml-cpp/yaml.h>

#include <string>
#include <vector>

void download_source(const YAML::Node pkg_config, const YAML::Node release,
                     const std::string package_name, const std::string source_type,
                     std::vector<std::string>& instructions);
std::string get_source_path(std::string env_path);