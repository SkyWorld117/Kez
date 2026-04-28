#include <dependency_resolver/advisor.hpp>
#include <filesystem>

#include "yaml-cpp/yaml.h"

std::string advise(const std::string& abstract_pkg) {
    std::filesystem::path config_path =
        std::filesystem::path(getenv("FROMAGER_HOME")) / "heuristics" / "advice.yaml";
    YAML::Node config = YAML::LoadFile(config_path)["advice"];

    std::string arch = getenv("FROMAGER_ARCH");

    if (config[abstract_pkg] && config[abstract_pkg][arch]) {
        return config[abstract_pkg][arch].as<std::string>();
    }

    ERROR("No advice available for package: " + abstract_pkg + " on architecture: " + arch);
    exit(EXIT_FAILURE);
}