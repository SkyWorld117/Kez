#include <algorithm>
#include <database/config_selector.hpp>
#include <database/errors.hpp>
#include <utils/string_utils.hpp>
#include <vector>

struct ConfigRange {
    std::string start;
    std::string end;
    std::filesystem::path path;
};

static int compare_range_versions(const std::string& left, const std::string& right,
                                  const std::filesystem::path& path) {
    try {
        return compare_versions(left, right);
    } catch (const std::exception& error) {
        raise_database_error("Cannot compare versions in '" + path.string() + "': " + error.what());
    }
}

static ConfigRange parse_range_path(const std::filesystem::path& path) {
    const std::string stem      = path.stem().string();
    const std::size_t separator = stem.find('-');
    if (separator == std::string::npos || separator == 0 || separator + 1 == stem.size() ||
        stem.find('-', separator + 1) != std::string::npos) {
        raise_database_error("Invalid database config filename '" + path.string() +
                             "'; expected latest.yaml or <start-version>-<end-version>.yaml");
    }

    ConfigRange range {stem.substr(0, separator), stem.substr(separator + 1), path};
    if (compare_range_versions(range.start, range.end, path) > 0) {
        raise_database_error("Invalid database config range '" + path.string() +
                             "': start version is greater than end version");
    }
    return range;
}

std::filesystem::path select_config_path(const std::filesystem::path& database_path,
                                         const std::string& package_name,
                                         const std::string& version) {
    const std::filesystem::path package_path = database_path / package_name;
    if (!std::filesystem::is_directory(package_path)) {
        raise_database_error("Package config directory not found: " + package_path.string());
    }

    const std::filesystem::path latest_path = package_path / "latest.yaml";
    if (!std::filesystem::is_regular_file(latest_path)) {
        raise_database_error("Latest package config not found: " + latest_path.string());
    }

    std::vector<ConfigRange> ranges;
    for (const auto& entry : std::filesystem::directory_iterator(package_path)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".yaml" ||
            entry.path().filename() == "latest.yaml") {
            continue;
        }
        ranges.push_back(parse_range_path(entry.path()));
    }

    std::sort(ranges.begin(), ranges.end(), [](const ConfigRange& left, const ConfigRange& right) {
        const int start_comparison = compare_range_versions(left.start, right.start, left.path);
        if (start_comparison != 0) {
            return start_comparison < 0;
        }
        return compare_range_versions(left.end, right.end, left.path) < 0;
    });
    for (std::size_t i = 1; i < ranges.size(); ++i) {
        if (compare_range_versions(ranges[i].start, ranges[i - 1].end, ranges[i].path) <= 0) {
            raise_database_error("Overlapping database config ranges: '" +
                                 ranges[i - 1].path.string() + "' and '" + ranges[i].path.string() +
                                 "'");
        }
    }

    if (version.empty() || version == "latest") {
        return latest_path;
    }
    for (const ConfigRange& range : ranges) {
        if (compare_range_versions(range.start, version, range.path) <= 0 &&
            compare_range_versions(version, range.end, range.path) <= 0) {
            return range.path;
        }
    }
    return latest_path;
}

void validate_package_name(const std::string& package_name) {
    const std::filesystem::path path(package_name);
    if (package_name.empty() || package_name == "." || package_name == ".." ||
        path.has_parent_path() || path.filename().string() != package_name) {
        raise_database_error("Invalid package name: '" + package_name + "'");
    }
}
