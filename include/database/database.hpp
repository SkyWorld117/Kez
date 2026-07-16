#pragma once

#include <database/config.hpp>
#include <filesystem>
#include <memory>
#include <string>

/**
 * @brief Shared pointer to an immutable PackageConfig.
 *
 * All database accessors return this type, allowing parsed configurations to
 * be cached and shared without copying.  Because the pointed-to object is
 * const, callers can safely read properties, dependencies, and build stages
 * without worrying about concurrent or accidental modification.
 */
using PackageConfigPtr = std::shared_ptr<const PackageConfig>;

/**
 * @brief Look up a specific version of a package from the database.
 *
 * Searches the database directory tree (pointed to by the KEZ_DB environment
 * variable) for the recipe matching @p package_name at @p version.  The
 * located YAML file is parsed into a PackageConfig, cached internally, and
 * returned.
 *
 * @param package_name  Name of the package (e.g. "openmpi", "hdf5").
 *                      Must be a non-empty, valid package identifier; an
 *                      invalid name triggers an error via validate_package_name().
 * @param version       Version string (e.g. "4.1.5", "latest").  If the
 *                      recipe file does not exist under
 *                      <tt>$KEZ_DB/&lt;name&gt;/&lt;version&gt;.yaml</tt>
 *                      the function terminates the program.
 * @return A shared pointer to the parsed PackageConfig.  Subsequent calls
 *         with the same resolved (absolute, normalised) path return the
 *         cached object without re-parsing.
 *
 * @note The internal cache key is the absolute, lexically-normalised path
 *       of the recipe file.  This means that two calls referring to the same
 *       file (e.g. via different relative paths or symlinks) will still hit
 *       the cache.
 *
 * @warning Aborts the program (via ERROR macro) if:
 *         - @p package_name fails validation,
 *         - the recipe file cannot be found at the expected path,
 *         - the YAML document is malformed or contains invalid fields.
 *
 * @see get_db_config(const std::string&)  Convenience overload defaulting to "latest".
 * @see parse_db_config()  Low-level parser that reads a file at an explicit path.
 * @see clear_db_cache()  Flush the internal parse cache.
 */
PackageConfigPtr get_db_config(const std::string& package_name, const std::string& version);

/**
 * @brief Look up the latest version of a package from the database.
 *
 * Equivalent to <tt>get_db_config(package_name, "latest")</tt>.
 *
 * @param package_name  Name of the package (e.g. "zlib", "fftw").
 * @return A shared pointer to the parsed PackageConfig for the "latest"
 *         version of the package.
 *
 * @see get_db_config(const std::string&, const std::string&)
 */
PackageConfigPtr get_db_config(const std::string& package_name);

/**
 * @brief Parse a single package recipe YAML file at an explicit filesystem path.
 *
 * Loads and fully parses the YAML document at @p config_path into a
 * PackageConfig object.  The result is **not** cached by this function;
 * callers that want caching should use get_db_config() instead.
 *
 * Every top-level section of the recipe is validated: source information,
 * build stages, configurations, options, environment variables, properties,
 * overrides, etc.  Semantic rules (e.g. abstract packages must have at least
 * one implementation) are enforced during parsing.
 *
 * @param config_path  Absolute or relative path to the recipe YAML file.
 * @return A shared pointer to the newly-parsed PackageConfig.  The returned
 *         object's dynamic type may be GenericPackageConfig, AutotoolsPackageConfig,
 *         CMakePackageConfig, or MakePackageConfig depending on the
 *         toolchain declared in the recipe.
 *
 * @warning Aborts the program (via ERROR/fail_config) if:
 *         - the file cannot be loaded or is not valid YAML,
 *         - required fields are missing,
 *         - field values violate the recipe schema (e.g. unknown enum
 *           values, mismatched types, abstract packages with no implementations).
 *
 * @see get_db_config()  Higher-level lookup that includes path resolution
 *                       and caching.
 */
PackageConfigPtr parse_db_config(const std::filesystem::path& config_path);

/**
 * @brief Clear the internal recipe parse cache.
 *
 * Removes all previously cached PackageConfig entries from the internal
 * hash map.  Subsequent calls to get_db_config() will re-parse the
 * corresponding YAML files from disk.
 *
 * This is useful in long-running processes (e.g. interactive sessions or
 * daemons) where the underlying recipe files on disk may have changed since
 * the process started.
 *
 * @note The cache is module-scoped (anonymous namespace) and is **not**
 *       thread-safe.  Callers in a multi-threaded context should synchronise
 *       access externally.
 *
 * @see get_db_config()  Populates the cache that this function clears.
 */
void clear_db_cache();

/**
 * @brief Resolve the best available version for a dependency given version
 *        constraints.
 *
 * Discovers available versions by scanning the package's database directory
 * for version-range files (e.g. ``6.1.3-6.1.3.yaml``) and reading the
 * @c source.releases list in ``latest.yaml``.  All candidate versions are
 * filtered against the supplied @p constraints and the latest (highest)
 * matching version is returned.
 *
 * If @p constraints is empty the function returns @c "latest" (no constraint).
 *
 * @param package_name  Name of the dependency package.
 * @param constraints   Version constraints to satisfy (ANDed together).
 *                      May be empty, meaning "any version is acceptable".
 * @return A version string suitable for passing to get_db_config(), or
 *         @c "latest" when no constraints are given.  Terminates the program
 *         if a constraint is present but no matching version can be found.
 */
std::string resolve_dependency_version(const std::string& package_name,
                                       const std::vector<DependencyConstraint>& constraints);
