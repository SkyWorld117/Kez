#pragma once

#include <string>
#include <unordered_map>
#include <vector>

using DependencyGraph = std::unordered_map<std::string, std::vector<std::string>>;

// The graph maps a package to its dependencies. Dependencies therefore appear
// before their dependents in the returned topological order.
std::vector<std::string> topological_sort(const DependencyGraph& adjacency_list);
