#include "optional_dependencies.h"

std::vector<std::string> get_optional_dependencies(const YAML::Node& node) {
    std::vector<std::string> optional_deps;

    if (node.IsScalar() || node.IsNull()) {
        return optional_deps; // No optional dependencies
    }

    if (node.IsMap()) {
        if (node["requires"]) {
            for (const auto& dep : node["requires"]) {
                optional_deps.push_back(dep.as<std::string>());
            }
        } else {
            for (const auto& item : node) {
                std::vector<std::string> deps = get_optional_dependencies(item.second);
                optional_deps.insert(optional_deps.end(), deps.begin(), deps.end());
            }
        }
    } else if (node.IsSequence()) {
        for (const auto& item : node) {
            std::vector<std::string> deps = get_optional_dependencies(item);
            optional_deps.insert(optional_deps.end(), deps.begin(), deps.end());
        }
    }

    return optional_deps;
}

std::vector<std::string> get_optional_dependencies(const std::string& pkg_name) {
    std::vector<std::string> optional_deps;

    std::filesystem::path db_path(getenv("CHEESE_DB"));
    std::filesystem::path config_file(pkg_name + ".yaml");
    std::filesystem::path config_path = db_path / config_file;

    if (!std::filesystem::exists(config_path)) {
        ERROR("Configuration file does not exist: " + config_path.string());
        return optional_deps;
    }

    YAML::Node config = YAML::LoadFile(config_path.string());
    optional_deps = get_optional_dependencies(config);

    return optional_deps;
}