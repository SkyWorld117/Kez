#include <dependency_resolver/essential_dependencies.hpp>

std::vector<std::string> get_essential_dependencies(const std::string& pkg_name) {
    std::vector<std::string> essential_deps;

    YAML::Node config = get_db_config(pkg_name);

    if (config["cheese"]["dependencies"]) {
        for (const auto& dep : config["cheese"]["dependencies"]) {
            essential_deps.push_back(dep.as<std::string>());
        }
    }

    return essential_deps;
}