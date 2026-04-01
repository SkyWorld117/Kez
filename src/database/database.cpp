#include <database/database.hpp>

std::unordered_map<std::string, YAML::Node> db_cache;
std::filesystem::path db_path(getenv("FROMAGER_DB"));

YAML::Node get_db_config(std::string pkg_name) {
    // Check if the config is already cached
    if (db_cache.find(pkg_name) != db_cache.end()) {
        return db_cache[pkg_name];
    }

    // Load the config from the YAML file
    std::filesystem::path config_path = db_path / (pkg_name + ".yaml");
    if (!std::filesystem::exists(config_path)) {
        ERROR("Config file not found: " + config_path.string());
        exit(EXIT_FAILURE);
    }
    YAML::Node config = YAML::LoadFile(config_path);

    // Cache the config
    db_cache[pkg_name] = config;

    return config;
}