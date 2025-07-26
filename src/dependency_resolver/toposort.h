#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <functional>

std::vector<std::string> topological_sort(const std::unordered_map<std::string, std::vector<std::string>>& adjacency_list);