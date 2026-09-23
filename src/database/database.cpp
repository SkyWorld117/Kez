#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cstdlib>
#include <database/config_selector.hpp>
#include <database/database.hpp>
#include <database/source_parser.hpp>
#include <unordered_map>
#include <unordered_set>
#include <utils/bash_utils.hpp>
#include <utils/colored_io.hpp>
#include <utils/string_utils.hpp>
#include <utils/yaml_utils.hpp>
#include <vector>

namespace {
    /**
     * @brief Module-level cache for parsed package configurations.
     *
     * Keyed by "<package_name>@<version>" (e.g. "openmpi@4.1.5").  This is a
     * simple per-lookup cache: it is very unlikely that the same package will
     * be requested with two different version strings in a single run, so there
     * is no cross-version deduplication.
     *
     * Populated lazily by get_db_config() and never trimmed; call
     * clear_db_cache() to evict all entries.
     *
     * @warning This cache is **not** thread-safe.  Concurrent access from
     *          multiple threads without external synchronisation is undefined
     *          behaviour.
     *
     * @see get_db_config()
     * @see clear_db_cache()
     */
    std::unordered_map<std::string, PackageConfigPtr> db_cache;

    /**
     * @brief Parse the start/end versions from a range filename's stem.
     *
     * A filename like ``6.1.3-6.1.3.yaml`` yields start = "6.1.3",
     * end = "6.1.3".  The caller is responsible for ensuring the path
     * exists and has a valid range format.
     *
     * @param stem  Filename stem (extension already removed).
     * @return A pair (start, end).
     */
    std::pair<std::string, std::string> parse_range_stem(const std::string& stem) {
        const std::size_t sep = stem.find('-');
        // This is only called for already-validated range files, so sep
        // is guaranteed to be valid.
        return {stem.substr(0, sep), stem.substr(sep + 1)};
    }

    /**
     * @brief Collect all known version strings for a package.
     *
     * Versions come from two sources:
     *   - Version-range YAML filenames (e.g. ``6.1.3-6.1.3.yaml`` contributes
     *     both "6.1.3" and "6.1.3" — we deduplicate afterwards).
     *   - The @c source.releases list in ``latest.yaml``.
     *
     * @param database_path  Root of the database tree (``$KEZ_DB``).
     * @param package_name   Package to enumerate versions for.
     * @return A vector of version strings, possibly empty.
     */
    std::vector<std::string> collect_available_versions(const std::string& package_name) {
        std::vector<PackageConfigPtr> configs = get_all_db_configs(package_name);
        std::vector<std::string> versions;
        for (const PackageConfigPtr& config : configs) {
            if (config->source) {
                for (const Release& release : config->source->releases) {
                    versions.push_back(release.version);
                }
            }
        }
        return versions;
    }

    /**
     * @brief Check whether a single version satisfies a constraint.
     *
     * @param version  The version to test.
     * @param constraint  The constraint (operator + version) to apply.
     * @return true if the version satisfies the constraint.
     */
    bool version_satisfies(const std::string& version, const DependencyConstraint& constraint) {
        const int cmp = compare_versions(version, constraint.version);
        if (constraint.op == ">=") return cmp >= 0;
        if (constraint.op == ">") return cmp > 0;
        if (constraint.op == "<=") return cmp <= 0;
        if (constraint.op == "<") return cmp < 0;
        if (constraint.op == "==") return cmp == 0;
        return false;  // Unknown operator — should not reach here.
    }

}  // namespace

PackageConfigPtr get_db_config(const std::string& package_name, const std::string& version) {
    std::string cache_key = package_name + "@" + version;
    const auto cached     = db_cache.find(cache_key);
    if (cached != db_cache.end()) {
        return cached->second;
    }

    validate_package_name(package_name);
    std::filesystem::path database_env = get_env_var("KEZ_DB");

    const std::filesystem::path config_path =
        select_config_path(database_env, package_name, version);

    PackageConfigPtr config         = parse_db_config(config_path);
    const auto [iterator, inserted] = db_cache.emplace(cache_key, config);
    return inserted ? config : iterator->second;
}

PackageConfigPtr get_db_config(const std::string& package_name) {
    return get_db_config(package_name, "latest");
}

std::vector<PackageConfigPtr> get_all_db_configs(const std::string& package_name) {
    validate_package_name(package_name);
    const std::filesystem::path package_path =
        std::filesystem::path(get_env_var("KEZ_DB")) / package_name;
    if (!std::filesystem::is_directory(package_path)) {
        ERROR("Package config directory not found: " + package_path.string());
        exit(EXIT_FAILURE);
    }

    std::vector<std::filesystem::path> paths;
    const std::filesystem::path latest_path = package_path / "latest.yaml";
    if (std::filesystem::is_regular_file(latest_path)) {
        paths.push_back(latest_path);
    }

    std::vector<std::pair<std::string, std::filesystem::path>> ranges;
    for (const auto& entry : std::filesystem::directory_iterator(package_path)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".yaml" ||
            entry.path().filename() == "latest.yaml") {
            continue;
        }
        const std::string stem      = entry.path().stem().string();
        const std::size_t separator = stem.find('-');
        if (separator == std::string::npos) {
            continue;
        }
        ranges.emplace_back(stem.substr(0, separator), entry.path());
    }
    std::sort(ranges.begin(), ranges.end(), [](const auto& left, const auto& right) {
        return compare_versions(left.first, right.first) > 0;
    });
    for (const auto& range : ranges) {
        paths.push_back(range.second);
    }

    std::vector<PackageConfigPtr> configs;
    configs.reserve(paths.size());
    for (const std::filesystem::path& path : paths) {
        configs.push_back(parse_db_config(path));
    }
    return configs;
}

void clear_db_cache() { db_cache.clear(); }

std::string resolve_dependency_version(const std::string& package_name,
                                       const std::vector<DependencyConstraint>& constraints) {
    // No constraints → use "latest".
    if (constraints.empty()) {
        return "latest";
    }

    validate_package_name(package_name);
    const std::filesystem::path database_path = get_env_var("KEZ_DB");

    std::vector<std::string> candidates = collect_available_versions(package_name);

    // Filter by all constraints.
    std::vector<std::string> matching;
    for (const std::string& version : candidates) {
        bool ok = true;
        for (const DependencyConstraint& constraint : constraints) {
            if (!version_satisfies(version, constraint)) {
                ok = false;
                break;
            }
        }
        if (ok) {
            matching.push_back(version);
        }
    }

    if (matching.empty()) {
        ERROR("No available version of '" + package_name + "' satisfies the required constraints");
        exit(EXIT_FAILURE);
    }

    // Sort descending and return the highest.
    std::sort(matching.begin(), matching.end(), [](const std::string& a, const std::string& b) {
        return compare_versions(a, b) > 0;
    });

    return matching.front();
}
