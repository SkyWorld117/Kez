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

/**
 * @brief Look up a specific version of a package from the database,
 *        caching the result.
 *
 * Validates @p package_name via validate_package_name(), resolves the path to
 * the recipe YAML file through select_config_path() (which consults the
 * KEZ_DB environment variable), and checks the module-scoped @ref db_cache
 * before parsing.  If the resolved absolute, normalised path is found in the
 * cache the stored PackageConfigPtr is returned immediately; otherwise the
 * file is parsed with parse_db_config(), inserted into the cache, and
 * returned.
 *
 * @param package_name  Name of the package (e.g. "openmpi", "hdf5").  Must
 *                      pass validate_package_name() or the program
 *                      terminates with an error.
 * @param version       Version string (e.g. "4.1.5", "latest").  Used by
 *                      select_config_path() to pick a version-range recipe
 *                      file or fall back to "latest.yaml".
 *
 * @return Shared pointer to a const PackageConfig.  Subsequent lookups of
 *         the same physical file (by absolute, normalised path) return the
 *         cached object without re-parsing.
 *
 * @note The cache key is built from the absolute, lexically-normalised path,
 *       so different relative or symlink-based references to the same file
 *       share a single cache entry.
 *
 * @warning Terminates the program if:
 *          - @p package_name fails validation,
 *          - the KEZ_DB environment variable is not set (get_env_var() calls
 *            ERROR() and exit()),
 *          - the recipe file cannot be found (select_config_path() errors),
 *          - the YAML document is malformed or violates the schema
 *            (parse_db_config() calls fail_config() / user_config_error()).
 *
 * @see get_db_config(const std::string&)  Convenience overload defaulting to
 *                                          "latest".
 * @see parse_db_config()  Low-level file parser invoked on cache misses.
 * @see clear_db_cache()   Evicts all entries from the cache.
 */
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

/**
 * @brief Look up the latest version of a package from the database.
 *
 * Convenience overload that delegates to the two-parameter version with
 * "latest" as the version string.  Equivalent to
 * <tt>get_db_config(package_name, "latest")</tt>.
 *
 * @param package_name  Name of the package (e.g. "zlib", "fftw").
 *
 * @return Shared pointer to the parsed PackageConfig for the "latest"
 *         version of @p package_name.
 *
 * @warning Terminates the program under the same conditions as the
 *          two-parameter overload.
 *
 * @see get_db_config(const std::string&, const std::string&)
 */
PackageConfigPtr get_db_config(const std::string& package_name) {
    return get_db_config(package_name, "latest");
}

/**
 * @brief Clear the internal recipe parse cache.
 *
 * Removes all entries from the module-scoped @ref db_cache unordered_map.
 * After this call, every subsequent get_db_config() invocation will re-parse
 * the corresponding YAML recipe files from disk.
 *
 * @note This is an O(n) operation on the number of cached entries.  Shared
 *       pointers whose reference count drops to zero are destroyed
 *       immediately, which may free the underlying PackageConfig memory.
 *
 * @warning Not thread-safe.  If get_db_config() or clear_db_cache() may be
 *          called concurrently from multiple threads, the caller must provide
 *          external synchronisation.
 *
 * @see get_db_config()  Populates the cache that this function clears.
 */
void clear_db_cache() { db_cache.clear(); }
