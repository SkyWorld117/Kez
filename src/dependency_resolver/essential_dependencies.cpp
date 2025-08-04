#include "essential_dependencies.h"

std::vector<std::string> get_essential_dependencies(const std::string& pkg_name) {
    std::vector<std::string> essential_deps;

    std::filesystem::path db_path(getenv("FROMAGER_DB"));
    std::filesystem::path config_file(pkg_name + ".yaml");
    std::filesystem::path config_path = db_path / config_file;

    if (!std::filesystem::exists(config_path)) {
        ERROR("Configuration file does not exist: " + config_path.string());
        return essential_deps;
    }

    YAML::Node config = YAML::LoadFile(config_path.string());
    if (config["cheese"]["dependencies"]) {
        for (const auto& dep : config["cheese"]["dependencies"]) {
            essential_deps.push_back(dep.as<std::string>());
        }
    }

    return essential_deps;
}