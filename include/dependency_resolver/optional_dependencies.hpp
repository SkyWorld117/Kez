#pragma once

#include <string>
#include <vector>

/**
 * @brief Collect all optional (conditional) dependencies for a given package.
 *
 * Queries the package's database configuration and extracts the union of all
 * package names listed in the `requires` fields of:
 *   - Environment variables defined at the build level
 *   - Build options defined at the build level
 *   - Environment variables defined in each build stage's configurations
 *   - Build options defined in each build stage's configurations
 *
 * These are "optional" dependencies because they are only needed when the
 * corresponding environment variable or build option is actually enabled by
 * the user in the generated configuration file -- as opposed to hard
 * dependencies listed in the package's top-level `dependencies` field.
 *
 * Duplicates are removed from the returned list.  The order among unique
 * entries is roughly the order they first appear during the traversal
 * (build-level configs first, then stage-level configs).
 *
 * @param package_name  The name of the package whose optional dependencies
 *                      should be retrieved (corresponds to a subdirectory
 *                      under the project's database/ directory).
 *
 * @return A vector of zero or more unique package names that are optional
 *         dependencies of the specified package.  An empty vector is returned
 *         if the package does not exist in the database or if its recipe
 *         defines no optional dependencies.
 *
 * @see get_db_config()            Source of the package configuration data.
 * @see EnvironmentVariable        Struct whose `requires` field contributes
 *                                 optional dependency entries.
 * @see BuildOption                Struct whose `requires` field contributes
 *                                 optional dependency entries.
 * @see BuildConfiguration         Aggregates environment variables and options
 *                                 that carry `requires` fields.
 * @see BuildStage                 May contain a `BuildConfiguration` with
 *                                 additional `requires` entries.
 */
std::vector<std::string> get_optional_dependencies(const std::string& package_name);
