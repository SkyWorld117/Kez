#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "../dependency_resolver/essential_dependencies.h"
#include "../dependency_resolver/optional_dependencies.h"

std::vector<std::string> get_unbuilt_dependencies(
    const YAML::Node& user_config, const YAML::Node& ins_yaml,
    const std::vector<std::string>& installed_packages, const std::string& target_package);