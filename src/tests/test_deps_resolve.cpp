#include <cassert>
#include <dependency_resolver/resolve_dependencies.hpp>
#include <iostream>
#include <string>
#include <vector>

int main() {
    INFO("Running dependency resolution tests...");

    std::vector<std::string> pkgs = {"openmpi"};

    // Non-interactive mode
    auto result = resolve_dependencies(pkgs, false);

    std::vector<std::string> all_deps      = result.first.first;
    std::vector<std::string> filtered_deps = result.first.second;
    auto abstract_pkgs                     = result.second;

    std::cout << "All dependencies count: " << all_deps.size() << std::endl;
    std::cout << "Filtered dependencies count: " << filtered_deps.size() << std::endl;

    // Just verifying that it runs without crashing and has some logical output structure.
    assert(all_deps.size() >= filtered_deps.size());

    SUCCESS("Dependency resolution tests passed!");
    return 0;
}