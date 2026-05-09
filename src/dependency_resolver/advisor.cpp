#include <yaml-cpp/yaml.h>

#include <dependency_resolver/advisor.hpp>
#include <filesystem>
#include <utils/bash_utils.hpp>

std::string advise(const std::string& abstract_pkg) {
    std::filesystem::path config_path =
        std::filesystem::path(get_env_var("FROMAGER_HOME")) / "heuristics" / "advice.yaml";
    YAML::Node config = YAML::LoadFile(config_path)["advice"];

    std::string arch = get_env_var("FROMAGER_ARCH");

    if (config[abstract_pkg] && config[abstract_pkg][arch]) {
        return config[abstract_pkg][arch].as<std::string>();
    }

    ERROR("No advice available for package: " + abstract_pkg + " on architecture: " + arch);
    exit(EXIT_FAILURE);
}