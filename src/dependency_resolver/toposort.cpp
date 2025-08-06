#include "toposort.h"

std::vector<std::string> topological_sort(const std::unordered_map<std::string, std::vector<std::string>>& adjacency_list) {
    std::vector<std::string> sorted;
    std::unordered_map<std::string, bool> visited;

    for (const auto& pair : adjacency_list) {
        const std::string& name = pair.first;
        if (!visited[name]) {
            std::function<void(const std::string&)> dfs = [&](const std::string& current) {
                visited[current] = true;
                // Check if current node has dependencies before accessing
                if (adjacency_list.find(current) != adjacency_list.end()) {
                    for (const auto& dep : adjacency_list.at(current)) {
                        if (!visited[dep]) {
                            dfs(dep);
                        }
                    }
                    sorted.push_back(current);
                }
            };
            dfs(name);
        }
    }

    return sorted;
}