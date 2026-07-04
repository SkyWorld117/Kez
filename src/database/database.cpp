#include <cstdlib>
#include <database/config_selector.hpp>
#include <database/database.hpp>
#include <database/errors.hpp>
#include <mutex>
#include <unordered_map>
#include <utils/bash_utils.hpp>

static std::unordered_map<std::string, PackageConfigPtr> db_cache;
static std::mutex db_cache_mutex;

PackageConfigPtr get_db_config(const std::string& package_name, const std::string& version) {
    validate_package_name(package_name);
    std::filesystem::path database_env = get_env_var("KEZ_DB");

    const std::filesystem::path config_path =
        select_config_path(database_env, package_name, version);
    const std::string cache_key =
        std::filesystem::absolute(config_path).lexically_normal().string();

    {
        std::lock_guard<std::mutex> lock(db_cache_mutex);
        const auto cached = db_cache.find(cache_key);
        if (cached != db_cache.end()) {
            return cached->second;
        }
    }

    PackageConfigPtr config = parse_db_config(config_path);
    std::lock_guard<std::mutex> lock(db_cache_mutex);
    const auto [iterator, inserted] = db_cache.emplace(cache_key, config);
    return inserted ? config : iterator->second;
}

PackageConfigPtr get_db_config(const std::string& package_name) {
    return get_db_config(package_name, "latest");
}

void clear_db_cache() {
    std::lock_guard<std::mutex> lock(db_cache_mutex);
    db_cache.clear();
}
