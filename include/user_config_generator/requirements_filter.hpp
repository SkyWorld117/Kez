#pragma once

#include <dependency_resolver/resolve_dependencies.hpp>
#include <string>
#include <vector>

bool requirements_satisfied(const std::vector<std::string>& requirements,
                            const std::vector<std::string>& all_dependencies,
                            const AbstractPackageSelections& abstract_packages);
