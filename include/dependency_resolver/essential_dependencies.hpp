#pragma once

#include <string>
#include <vector>

/**
 * @brief Retrieves the essential (hard) dependency list for a given package.
 *
 * Looks up the package recipe from the database and returns its direct
 * dependencies as declared in the `dependencies` field of the recipe YAML.
 * These are the packages that must be installed before the named package can
 * be built or used.
 *
 * Essential dependencies are distinct from optional dependencies: the former
 * are always required (declared unconditionally in the recipe), while the
 * latter are inferred from the `requires` fields of environment variables and
 * build options (see get_optional_dependencies()).
 *
 * @param package_name  The name of the package whose essential dependencies
 *                      should be retrieved. Must correspond to a directory in
 *                      the database (e.g., `database/<package_name>/latest.yaml`).
 *
 * @return A vector of package names that this package directly depends on.
 *         Returns an empty vector if the package has no dependencies or if
 *         the database lookup fails (the underlying get_db_config() terminates
 *         the program on error).
 *
 * @see get_optional_dependencies() for conditionally required dependencies.
 * @see get_db_config() in database/database.hpp for the config lookup.
 * @see PackageConfig::dependencies in database/config.hpp for the underlying
 *      data member.
 */
std::vector<std::string> get_essential_dependencies(const std::string& package_name);
