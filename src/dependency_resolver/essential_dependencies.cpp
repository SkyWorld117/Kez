#include <database/database.hpp>
#include <dependency_resolver/essential_dependencies.hpp>

/**
 * @brief Retrieves the essential (hard) dependency list for a given package.
 *
 * Delegates to get_db_config() to look up the package recipe, then extracts the
 * `dependencies` field from the returned PackageConfig. These are the packages
 * that must be present before the named package can be built or used — they are
 * declared unconditionally in the recipe's YAML and are always required.
 *
 * @param package_name  The name of the package whose essential dependencies
 *                      should be retrieved. Must correspond to a directory in
 *                      the database (e.g., `database/<package_name>/latest.yaml`).
 *
 * @return A vector of package names that this package directly depends on.
 *         Returns an empty vector if the recipe declares no dependencies.
 *
 * @warning If get_db_config() cannot find the package or fails to parse its
 *          recipe, it will print an error via ERROR() and terminate the program
 *          with exit(EXIT_FAILURE). This function itself performs no additional
 *          error handling.
 *
 * @see get_optional_dependencies() for conditionally required dependencies.
 * @see get_db_config() in database/database.hpp for the config lookup.
 * @see PackageConfig::dependencies in database/config.hpp for the underlying
 *      data member.
 */
std::vector<std::string> get_essential_dependencies(const std::string& package_name) {
    return get_db_config(package_name)->dependencies;
}
