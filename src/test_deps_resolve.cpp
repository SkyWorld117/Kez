#include "dependency_resolver/resolve_dependencies.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <package_name>" << std::endl;
        return 1;
    }

    std::string pkg_name = argv[1];

    std::vector<std::string> dependencies = resolve_dependencies(pkg_name);
    if (dependencies.empty()) {
        std::cout << "No dependencies found for package: " << pkg_name << std::endl;
    } else {
        std::cout << "Dependencies for package '" << pkg_name << "':" << std::endl;
        for (const auto& dep : dependencies) {
            std::cout << "- " << dep << std::endl;
        }
    }

    // std::vector<std::string> filtered_dependencies = resolve_filtered_dependencies(pkg_name);
    // if (filtered_dependencies.empty()) {
    //     std::cout << "No filtered dependencies found for package: " << pkg_name << std::endl;
    // } else {
    //     std::cout << "Filtered dependencies for package '" << pkg_name << "':" << std::endl;
    //     for (const auto& dep : filtered_dependencies) {
    //         std::cout << "- " << dep << std::endl;
    //     }
    // }

    return 0;
}