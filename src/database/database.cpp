#include <cstdlib>
#include <database/config_selector.hpp>
#include <database/database.hpp>
#include <unordered_map>
#include <utils/bash_utils.hpp>

namespace {
    /**
     * @brief Module-level cache for parsed package configurations.
     *
     * Maps an absolute, lexically-normalised filesystem path (as a string) to
     * the corresponding parsed PackageConfig shared pointer.  Populated lazily
     * by get_db_config() and never trimmed; call clear_db_cache() to evict all
     * entries.
     *
     * @warning This cache is **not** thread-safe.  Concurrent access from
     *          multiple threads without external synchronisation is undefined
     *          behaviour.
     *
     * @see get_db_config()
     * @see clear_db_cache()
     */
    std::unordered_map<std::string, PackageConfigPtr> db_cache;
}  // namespace

PackageConfigPtr get_db_config(const std::string& package_name, const std::string& version) {
    validate_package_name(package_name);
    std::filesystem::path database_env = get_env_var("KEZ_DB");

    const std::filesystem::path config_path =
        select_config_path(database_env, package_name, version);
    const std::string cache_key =
        std::filesystem::absolute(config_path).lexically_normal().string();

    const auto cached = db_cache.find(cache_key);
    if (cached != db_cache.end()) {
        return cached->second;
    }

    PackageConfigPtr config         = parse_db_config(config_path);
    const auto [iterator, inserted] = db_cache.emplace(cache_key, config);
    return inserted ? config : iterator->second;
}

PackageConfigPtr get_db_config(const std::string& package_name) {
    return get_db_config(package_name, "latest");
}

void clear_db_cache() { db_cache.clear(); }
