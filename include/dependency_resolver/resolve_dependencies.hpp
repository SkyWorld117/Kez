#pragma once

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using DependencyLists           = std::pair<std::vector<std::string>, std::vector<std::string>>;
using AbstractPackageSelections = std::unordered_map<std::string, std::string>;
using DependencyResolution      = std::pair<DependencyLists, AbstractPackageSelections>;

// Returns ((all packages, non-system packages), abstract package selections).
// Both package lists use the legacy resolver's dependent-before-dependency order.
DependencyResolution resolve_dependencies(const std::vector<std::string>& package_names,
                                          bool interactive);
