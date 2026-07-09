#include <algorithm>
#include <cstdlib>
#include <database/config_selector.hpp>
#include <utils/colored_io.hpp>
#include <utils/string_utils.hpp>
#include <vector>

/**
 * @brief Describes a version-range config file for a package.
 *
 * Each instance represents one `<start>-<end>.yaml` file found inside a
 * package's database directory.  The range is inclusive on both ends.
 */
struct ConfigRange {
    std::string start;           ///< Lower bound of the version range (inclusive).
    std::string end;             ///< Upper bound of the version range (inclusive).
    std::filesystem::path path;  ///< Filesystem path to the config YAML file.
};

namespace {
    /**
     * @brief Parse a version-range filename into a ConfigRange struct.
     *
     * The filename stem must have the form `<start-version>-<end-version>`
     * (e.g. "1.0-2.0.yaml").  Leading/trailing hyphens, empty segments, and
     * extra hyphens are all rejected.
     *
     * @param path  Filesystem path to the config file (only the stem is parsed).
     * @return ConfigRange  Struct with start, end, and the original path.
     *
     * @note Terminates the program via ERROR() + exit(EXIT_FAILURE) if:
     *       - The stem contains zero hyphens, a leading/trailing hyphen, or
     *         more than one hyphen (i.e. does not match `<v1>-<v2>.yaml`).
     *       - @p start is version-greater than @p end (i.e. the range is reversed).
     */
    ConfigRange parse_range_path(const std::filesystem::path& path) {
        const std::string stem      = path.stem().string();
        const std::size_t separator = stem.find('-');
        if (separator == std::string::npos || separator == 0 || separator + 1 == stem.size() ||
            stem.find('-', separator + 1) != std::string::npos) {
            ERROR("Invalid database config filename '" + path.string() +
                  "'; expected latest.yaml or <start-version>-<end-version>.yaml");
            exit(EXIT_FAILURE);
        }

        ConfigRange range {stem.substr(0, separator), stem.substr(separator + 1), path};
        if (compare_versions(range.start, range.end) > 0) {
            ERROR("Invalid database config range '" + path.string() +
                  "': start version is greater than end version");
            exit(EXIT_FAILURE);
        }
        return range;
    }
}  // namespace

/**
 * @brief Select the best-matching config YAML for a given package and version.
 *
 * Searches the package's database directory for version-range files
 * (`<start>-<end>.yaml`), validates they are disjoint and sorted, then
 * returns the path whose range contains @p version.  Falls back to
 * `latest.yaml` if no range matches or @p version is empty / "latest".
 */
std::filesystem::path select_config_path(const std::filesystem::path& database_path,
                                         const std::string& package_name,
                                         const std::string& version) {
    const std::filesystem::path package_path = database_path / package_name;
    if (!std::filesystem::is_directory(package_path)) {
        ERROR("Package config directory not found: " + package_path.string());
        exit(EXIT_FAILURE);
    }

    const std::filesystem::path latest_path = package_path / "latest.yaml";
    if (!std::filesystem::is_regular_file(latest_path)) {
        ERROR("Latest package config not found: " + latest_path.string());
        exit(EXIT_FAILURE);
    }

    // Gather all `start-end.yaml` range files (skip latest.yaml itself).
    std::vector<ConfigRange> ranges;
    for (const auto& entry : std::filesystem::directory_iterator(package_path)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".yaml" ||
            entry.path().filename() == "latest.yaml") {
            continue;
        }
        ranges.push_back(parse_range_path(entry.path()));
    }

    // Sort ranges by (start, end) in lexicographic version order.
    std::sort(ranges.begin(), ranges.end(), [](const ConfigRange& left, const ConfigRange& right) {
        const int start_comparison = compare_versions(left.start, right.start);
        if (start_comparison != 0) {
            return start_comparison < 0;
        }
        return compare_versions(left.end, right.end) < 0;
    });

    // Validate that no adjacent ranges overlap (they must be disjoint).
    for (std::size_t i = 1; i < ranges.size(); ++i) {
        if (compare_versions(ranges[i].start, ranges[i - 1].end) <= 0) {
            ERROR("Overlapping database config ranges: '" + ranges[i - 1].path.string() +
                  "' and '" + ranges[i].path.string() + "'");
            exit(EXIT_FAILURE);
        }
    }

    // Empty / "latest" version -> return latest.yaml directly.
    if (version.empty() || version == "latest") {
        return latest_path;
    }

    // Search for a range that contains the requested version.
    for (const ConfigRange& range : ranges) {
        if (compare_versions(range.start, version) <= 0 &&
            compare_versions(version, range.end) <= 0) {
            return range.path;
        }
    }

    // No matching range found; fall back to the default config.
    return latest_path;
}

/**
 * @brief Validate that a package name is a plain, non-empty leaf name.
 *
 * Rejects empty strings, `.`, `..`, names with a parent path, and names
 * that the filesystem would interpret as a path (e.g. containing `/`).
 */
void validate_package_name(const std::string& package_name) {
    const std::filesystem::path path(package_name);
    if (package_name.empty() || package_name == "." || package_name == ".." ||
        path.has_parent_path() || path.filename().string() != package_name) {
        ERROR("Invalid package name: '" + package_name + "'");
        exit(EXIT_FAILURE);
    }
}
