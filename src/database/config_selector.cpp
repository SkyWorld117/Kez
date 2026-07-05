#include <algorithm>
#include <cstdlib>
#include <database/config_selector.hpp>
#include <utils/colored_io.hpp>
#include <utils/string_utils.hpp>
#include <vector>

struct ConfigRange {
    std::string start;
    std::string end;
    std::filesystem::path path;
};

namespace {
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

    std::vector<ConfigRange> ranges;
    for (const auto& entry : std::filesystem::directory_iterator(package_path)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".yaml" ||
            entry.path().filename() == "latest.yaml") {
            continue;
        }
        ranges.push_back(parse_range_path(entry.path()));
    }

    std::sort(ranges.begin(), ranges.end(), [](const ConfigRange& left, const ConfigRange& right) {
        const int start_comparison = compare_versions(left.start, right.start);
        if (start_comparison != 0) {
            return start_comparison < 0;
        }
        return compare_versions(left.end, right.end) < 0;
    });
    for (std::size_t i = 1; i < ranges.size(); ++i) {
        if (compare_versions(ranges[i].start, ranges[i - 1].end) <= 0) {
            ERROR("Overlapping database config ranges: '" + ranges[i - 1].path.string() +
                  "' and '" + ranges[i].path.string() + "'");
            exit(EXIT_FAILURE);
        }
    }

    if (version.empty() || version == "latest") {
        return latest_path;
    }
    for (const ConfigRange& range : ranges) {
        if (compare_versions(range.start, version) <= 0 &&
            compare_versions(version, range.end) <= 0) {
            return range.path;
        }
    }
    return latest_path;
}

void validate_package_name(const std::string& package_name) {
    const std::filesystem::path path(package_name);
    if (package_name.empty() || package_name == "." || package_name == ".." ||
        path.has_parent_path() || path.filename().string() != package_name) {
        ERROR("Invalid package name: '" + package_name + "'");
        exit(EXIT_FAILURE);
    }
}
