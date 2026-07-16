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
    std::vector<std::string> collect_available_versions(const std::filesystem::path& database_path,
                                                        const std::string& package_name) {
        std::unordered_set<std::string> seen;
        std::vector<std::string> versions;
        const std::filesystem::path package_dir = database_path / package_name;

        // 1. Scan version-range YAML files.
        if (std::filesystem::is_directory(package_dir)) {
            for (const auto& entry : std::filesystem::directory_iterator(package_dir)) {
                if (!entry.is_regular_file() || entry.path().extension() != ".yaml" ||
                    entry.path().filename() == "latest.yaml") {
                    continue;
                }
                const std::string stem = entry.path().stem().string();
                // Quick check for a valid range pattern before we try to parse.
                if (stem.find('-') == std::string::npos) continue;

                const auto [start, end] = parse_range_stem(stem);
                for (const std::string& v : {start, end}) {
                    if (seen.emplace(v).second) {
                        versions.push_back(v);
                    }
                }
            }
        }

        // 2. Read releases from latest.yaml.
        const std::filesystem::path latest_path = package_dir / "latest.yaml";
        if (std::filesystem::is_regular_file(latest_path)) {
            YAML::Node doc;
            try {
                doc = YAML::LoadFile(latest_path.string());
            } catch (...) {
                return versions;
            }
            if (doc.IsMap() && doc["recipe"].IsMap()) {
                const YAML::Node recipe = doc["recipe"];
                if (recipe["source"].IsMap() && recipe["source"]["releases"].IsSequence()) {
                    for (const YAML::Node& release : recipe["source"]["releases"]) {
                        if (release["version"].IsScalar()) {
                            const std::string version = release["version"].Scalar();
                            if (seen.emplace(version).second) {
                                versions.push_back(version);
                            }
                        }
                    }
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

void clear_db_cache() { db_cache.clear(); }

std::string resolve_dependency_version(const std::string& package_name,
                                       const std::vector<DependencyConstraint>& constraints) {
    // No constraints → use "latest".
    if (constraints.empty()) {
        return "latest";
    }

    validate_package_name(package_name);
    const std::filesystem::path database_path = get_env_var("KEZ_DB");

    std::vector<std::string> candidates = collect_available_versions(database_path, package_name);

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
