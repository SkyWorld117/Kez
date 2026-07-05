#pragma once

#include <dependency_resolver/resolve_dependencies.hpp>
#include <string>
#include <unordered_set>
#include <vector>

bool requirements_satisfied(const std::vector<std::string>& requirements,
                            const std::vector<std::string>& dependencies,
                            const AbstractPackageSelections& abstract_packages);

bool requirements_satisfied(const std::vector<std::string>& requirements,
                            const std::unordered_set<std::string>& dependencies,
                            const AbstractPackageSelections& abstract_packages);
