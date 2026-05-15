#include <database/database.hpp>
#include <filesystem>
#include <utils/bash_utils.hpp>

std::unordered_map<std::string, YAML::Node> db_cache;

YAML::Node get_db_config(const std::string& pkg_name) {
    if (auto it = db_cache.find(pkg_name); it != db_cache.end()) {
        return it->second;
    }

    std::filesystem::path db_path(get_env_var("FROMAGER_DB"));

    std::filesystem::path config_path = db_path / (pkg_name + ".yaml");
    if (!std::filesystem::exists(config_path)) {
        ERROR("Config file not found: " + config_path.string());
        exit(EXIT_FAILURE);
    }

    YAML::Node config  = YAML::LoadFile(config_path);
    db_cache[pkg_name] = config;
    return config;
}