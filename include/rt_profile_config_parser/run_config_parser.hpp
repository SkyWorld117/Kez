#pragma once

#include <yaml-cpp/yaml.h>

#include <utils/colored_io.hpp>
#include <rt_profile_config_parser/resource_manager.hpp>
#include <string>
#include <utility>
#include <vector>

std::pair<YAML::Node, std::string> parse_run_config(const YAML::Node& factory_config,
                                                    const YAML::Node& cellar_config,
                                                    const YAML::Node& profile_config);