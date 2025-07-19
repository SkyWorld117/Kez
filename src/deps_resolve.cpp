#include "deps_resolve.h"

std::filesystem::path db_path(getenv("CHEESE_DB"));
std::unordered_map<std::string, std::vector<std::string>> adjacency_list;
std::unordered_map<std::string, bool> system_package;

void get_dependencies(const std::string& pkg_name) {
    if (adjacency_list.find(pkg_name) != adjacency_list.end()) {
        return; // Already processed
    }

    std::filesystem::path config_file(pkg_name + ".yaml");
    std::filesystem::path config_path = db_path / config_file;

    if (!std::filesystem::exists(config_path)) {
        std::cerr << "Configuration file does not exist: " << config_path << std::endl;
        return;
    }

    YAML::Node config = YAML::LoadFile(config_path.string());

    system_package[pkg_name] = config["cheese"]["type"].as<std::string>() == "system";

    std::vector<std::string> dependencies;

    for (const auto& dep : config["cheese"]["dependencies"]) {
        dependencies.push_back(dep.as<std::string>());
        get_dependencies(dep.as<std::string>());
    }

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
        if (!system_package[dep]) {
            filtered_dependencies.push_back(dep);
        }
    }

    return filtered_dependencies;
}