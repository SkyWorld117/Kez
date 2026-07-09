#pragma once

#include <filesystem>
#include <string>

/**
 * @brief Select the best-matching configuration file for a given package and version.
 *
 * Searches the package's directory under `database_path/<package_name>/` for
 * a version-range file (e.g., `1.0-2.0.yaml`) whose range contains the requested
 * `version`.  If `version` is empty or equals `"latest"`, the `latest.yaml` file
 * is returned immediately without scanning ranges.
 *
 * All range files (every `.yaml` file except `latest.yaml`) are collected,
 * parsed into `[start, end]` intervals, sorted by (start, end), and checked for
 * overlaps.  Overlapping ranges are a fatal error.  The first range that contains
 * the requested version wins; if no range matches, `latest.yaml` is returned as
 * a fallback.
 *
 * @param database_path  Root directory of the package database (e.g. `database/`).
 * @param package_name   Name of the package whose config is being selected.
 * @param version        Target version string.  Pass an empty string or
 *                       `"latest"` to unconditionally return `latest.yaml`.
 * @return The path to the selected YAML configuration file.
 *
 * @warning Terminates the program with a non-zero exit code if:
 *          - The package directory does not exist.
 *          - `latest.yaml` is missing from the package directory.
 *          - Any range filename is malformed (not `latest.yaml` and not
 *            `<start>-<end>.yaml`).
 *          - A range's start is greater than its end.
 *          - Two ranges overlap.
 *
 * @see validate_package_name  Used internally to ensure `package_name` is safe.
 * @see compare_versions       Used to compare version strings when sorting
 *                             ranges and checking containment.
 */
std::filesystem::path select_config_path(const std::filesystem::path& database_path,
                                         const std::string& package_name,
                                         const std::string& version);

/**
 * @brief Validate that a string is a safe, simple package name.
 *
 * Rejects names that are empty, equal to `"."` or `".."`, contain a parent
 * path component, or differ from their own `filename()` (which catches
 * trailing slashes and path separators).  This prevents directory-traversal
 * attacks or filesystem ambiguities when the name is used to build a
 * filesystem path.
 *
 * @param package_name  The candidate package name to validate.
 *
 * @warning Terminates the program with a non-zero exit code if the name is
 *          invalid.
 *
 * @see select_config_path  Calls this function before looking up a package's
 *                          configuration directory.
 */
void validate_package_name(const std::string& package_name);
