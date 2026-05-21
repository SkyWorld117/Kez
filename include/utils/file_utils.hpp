#pragma once

#include <yaml-cpp/yaml.h>
#include <string>

std::string read_file(const std::string& path);

void write_yaml(const YAML::Node& node, const std::string& path);
void write_yaml(const YAML::Node& node, const std::string& path, const std::string& success_message);