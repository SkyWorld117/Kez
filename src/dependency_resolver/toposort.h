#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

std::vector<std::string> topological_sort(
    const std::unordered_map<std::string, std::vector<std::string>>& adjacency_list);