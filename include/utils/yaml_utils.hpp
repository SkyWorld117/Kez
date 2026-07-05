#pragma once

#include <yaml-cpp/yaml.h>

#include <string>

bool yaml_has(const YAML::Node& node, const std::string& key);
std::string yaml_scalar(const YAML::Node& node, const std::string& description);
bool yaml_boolean(const YAML::Node& node, const std::string& description);
