#pragma once

#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <string>

bool yaml_has(const YAML::Node& node, const std::string& key);
std::string yaml_scalar(const YAML::Node& node, const std::string& description);
bool yaml_boolean(const YAML::Node& node, const std::string& description);

// Load a YAML file, caching the result by absolute path so the same file is
// never parsed more than once per process lifetime. Errors are fatal as usual.
YAML::Node cached_yaml_load(const std::filesystem::path& path);
