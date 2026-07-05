#pragma once

#include <yaml-cpp/yaml.h>

#include <database/config.hpp>
#include <dependency_resolver/resolve_dependencies.hpp>
#include <string>
#include <vector>

YAML::Node filtered_environment(const std::vector<EnvironmentVariable>& environment,
                                const std::vector<std::string>& all_dependencies,
                                const AbstractPackageSelections& abstract_packages);

YAML::Node filtered_environment(const std::vector<EnvironmentVariable>& environment,
                                const std::vector<std::string>& all_dependencies);
