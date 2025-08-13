#include "../colors/colored_io.h"
#include "../dependency_resolver/resolve_dependencies.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        ERROR("Usage: " + std::string(argv[0]) + " <package_name>");
        return 1;
    }

    std::string pkg_name = argv[1];

    std::pair<std::pair<std::vector<std::string>, std::vector<std::string>>, std::unordered_map<std::string, std::string>> result = resolve_dependencies(pkg_name);
    std::vector<std::string> all_dependencies = result.first.first;
    std::vector<std::string> dependencies = result.first.second;
    std::unordered_map<std::string, std::string> abstract_packages = result.second;
    if (dependencies.empty()) {
        ERROR("No dependencies found for package: " + pkg_name);
        return 1;
    }

    std::cout << "All dependencies for package '" << pkg_name << "':" << std::endl;
    for (const auto& dep : all_dependencies) {
        std::cout << " - " << dep << std::endl;
    }

    std::cout << "Filtered dependencies for package '" << pkg_name << "':" << std::endl;
    for (const auto& dep : dependencies) {
        std::cout << " - " << dep << std::endl;
    }

    std::cout << "Abstract packages:" << std::endl;
    for (const auto& pair : abstract_packages) {
        std::cout << " - " << pair.first << " resolved to " << pair.second << std::endl;
    }

    return 0;
}