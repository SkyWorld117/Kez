#include "deps_resolve.h"

std::filesystem::path db_path(getenv("CHEESE_DB"));
std::unordered_map<std::string, std::vector<std::string>> adjacency_list;
std::unordered_map<std::string, bool> system_packages;
std::unordered_map<std::string, bool> abstract_packages;

// Optional dependencies start with `requires: [<dep1>, <dep2>]`
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

void get_dependencies(const std::string& pkg_name) {
    if (adjacency_list.find(pkg_name) != adjacency_list.end()) {
        return; // Already processed
    }

    std::filesystem::path config_file(pkg_name + ".yaml");
    std::filesystem::path config_path = db_path / config_file;

    if (!std::filesystem::exists(config_path)) {
        ERROR("Configuration file does not exist: " + config_path.string());
        return;
    }

    YAML::Node config = YAML::LoadFile(config_path.string());

    system_packages[pkg_name] = config["cheese"]["type"].as<std::string>() == "system";

    std::vector<std::string> dependencies;

    if (config["cheese"]["dependencies"]) {
        for (const auto& dep : config["cheese"]["dependencies"]) {
            dependencies.push_back(dep.as<std::string>());
            get_dependencies(dep.as<std::string>());
        }
    }

    std::vector<std::string> optional_dependencies = get_optional_dependencies(config);
    dependencies.insert(dependencies.end(), optional_dependencies.begin(), optional_dependencies.end());

    adjacency_list[pkg_name] = dependencies;
}

std::vector<std::string> resolve_dependencies(const std::string& pkg_name) {
    get_dependencies(pkg_name);

    std::vector<std::string> ordered_dependencies;
    std::unordered_map<std::string, bool> visited;

    for (const auto& pair : adjacency_list) {
        const std::string& name = pair.first;
        if (!visited[name]) {
            std::function<void(const std::string&)> dfs = [&](const std::string& current) {
                visited[current] = true;
                for (const auto& dep : adjacency_list[current]) {
                    if (!visited[dep]) {
                        dfs(dep);
                    }
                }
                ordered_dependencies.push_back(current);
            };
            dfs(name);
        }
    }

    std::reverse(ordered_dependencies.begin(), ordered_dependencies.end());
    return ordered_dependencies;
}

std::vector<std::string> resolve_filtered_dependencies(const std::string& pkg_name) {
    std::vector<std::string> all_dependencies = resolve_dependencies(pkg_name);
    std::vector<std::string> filtered_dependencies;

    for (const auto& dep : all_dependencies) {
        if (!system_packages[dep]) {
            filtered_dependencies.push_back(dep);
        }
    }

    return filtered_dependencies;
}