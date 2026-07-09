#include <database/database.hpp>
#include <dependency_resolver/essential_dependencies.hpp>

/** @brief Retrieves the essential (hard) dependencies for a given package by looking up its database config. */
std::vector<std::string> get_essential_dependencies(const std::string& package_name) {
    return get_db_config(package_name)->dependencies;
}
