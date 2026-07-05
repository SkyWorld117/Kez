#pragma once

#include <yaml-cpp/yaml.h>

#include <string>
#include <vector>

YAML::Node gen_user_config(const std::vector<std::string>& package_names, bool interactive);

YAML::Node gen_user_config(const std::vector<std::string>& package_names, bool interactive,
                           const std::string& default_compiler);
