#include <database/database.hpp>
#include <dependency_resolver/essential_dependencies.hpp>

std::vector<std::string> get_essential_dependencies(const std::string& package_name) {
    return get_db_config(package_name)->dependencies;
}
