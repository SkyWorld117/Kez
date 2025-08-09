#include "resolve_dependencies.h"

std::filesystem::path db_path(getenv("FROMAGER_DB"));
std::unordered_map<std::string, std::vector<std::string>> adjacency_list;
std::unordered_map<std::string, bool> system_packages;
std::unordered_map<std::string, bool> compiler_packages;
std::unordered_map<std::string, bool> mpi_packages;
std::unordered_map<std::string, std::string> abstract_packages;
std::unordered_map<std::string, bool> use_optional_packages;

void build_adjacency_list(const std::string& pkg_name, const std::string& target_pkg_name, const bool filter_pkg) {
    if (adjacency_list.find(pkg_name) != adjacency_list.end()) {
        return; // Already processed
    }

    std::filesystem::path config_file(pkg_name + ".yaml");
    std::filesystem::path config_path = db_path / config_file;

    if (!std::filesystem::exists(config_path)) {
        ERROR("Configuration file does not exist: " + config_path.string());
        exit(EXIT_FAILURE);
    }

    YAML::Node config = YAML::LoadFile(config_path.string());

    std::string concrete_pkg_name;
    
    // Ask for user selection if the package is abstract and not already selected
    if (config["cheese"]["type"].as<std::string>() == "abstract") {
        if (abstract_packages.find(pkg_name) == abstract_packages.end()) {
            INFO("Package '" + pkg_name + "' is abstract. Please select one of the following implementations:");
            for (const auto& impl : config["cheese"]["implementations"]) {
                std::string impl_name = impl.as<std::string>();
                INFO("- " + impl_name);
            }
            std::string selected_impl;
            while (true) {
                // Prompt user for implementation selection
                INFO("Enter the implementation name: ");
                std::getline(std::cin, selected_impl);
                // Remove leading/trailing whitespace/newline etc.
                selected_impl.erase(0, selected_impl.find_first_not_of(" \n\r\t"));
                selected_impl.erase(selected_impl.find_last_not_of(" \n\r\t") + 1);
                // Validate the selected implementation
                std::vector<std::string> implementations = config["cheese"]["implementations"].as<std::vector<std::string>>();
                if (std::find(implementations.begin(), implementations.end(), selected_impl) == implementations.end()) {
                    ERROR("Invalid implementation selected: " + selected_impl);
                    continue; // Ask again
                }
                abstract_packages[pkg_name] = selected_impl;
                concrete_pkg_name = selected_impl; // Use the selected implementation for further processing
                break;
            }
        } else {
            concrete_pkg_name = abstract_packages[pkg_name]; // Use the already selected implementation
        }

        if (adjacency_list.find(concrete_pkg_name) != adjacency_list.end()) {
            return; // Already processed
        }

        std::filesystem::path concrete_pkg_path = db_path / (concrete_pkg_name + ".yaml");
        if (!std::filesystem::exists(concrete_pkg_path)) {
            ERROR("Configuration file for selected implementation does not exist: " + concrete_pkg_path.string());
            exit(EXIT_FAILURE);
        }
        config = YAML::LoadFile(concrete_pkg_path.string());
    } else {
        concrete_pkg_name = pkg_name; // Use the original package name for non-abstract packages
    }

    if (filter_pkg) {
        if (config["cheese"]["type"].as<std::string>() == "system") {
            return; // Skip system packages
        } else if ((config["cheese"]["type"].as<std::string>() == "compiler" ||
                    config["cheese"]["type"].as<std::string>() == "mpi") &&
                   concrete_pkg_name != target_pkg_name) {
            adjacency_list[concrete_pkg_name] = {}; // Skip compiler and MPI packages that are not the target
            return;
        }
    }

    // Get essential and optional dependencies
    std::vector<std::string> essential_deps = get_essential_dependencies(concrete_pkg_name);
    std::vector<std::string> optional_deps = get_optional_dependencies(concrete_pkg_name);

    // Ask for user selection for optional dependencies
    if (!optional_deps.empty()) {
        bool has_optional = false;
        for (const auto& dep : optional_deps) {
            if (use_optional_packages.find(dep) == use_optional_packages.end()) {
                if (!has_optional) {
                    INFO("Optional dependencies for '" + concrete_pkg_name + "':");
                    has_optional = true;
                }
                INFO("- Include optional dependency: " + dep + "? (y/n)");
                std::string choice;
                std::getline(std::cin, choice);
                if (choice == "y" || choice == "Y") {
                    essential_deps.push_back(dep);
                    use_optional_packages[dep] = true;
                } else {
                    use_optional_packages[dep] = false;
                }
            } else if (use_optional_packages[dep]) {
                essential_deps.push_back(dep); // Already selected, add to essential deps
            }
        }
    }

    adjacency_list[concrete_pkg_name] = essential_deps;

    for (const auto& dep : essential_deps) {
        build_adjacency_list(dep, target_pkg_name, filter_pkg);
    }
}

std::vector<std::string> resolve_dependencies(const std::string& pkg_name, const bool filter_pkg) {
    adjacency_list.clear();
    build_adjacency_list(pkg_name, pkg_name, filter_pkg);

    // Unify the adjacency list to ensure all abstract packages are resolved to their selected implementations
    std::unordered_map<std::string, std::vector<std::string>> unified_adjacency_list;
    for (const auto& pair : adjacency_list) {
        const std::string& name = pair.first;
        std::string resolved_name = name;
        if (abstract_packages.find(name) != abstract_packages.end()) {
            resolved_name = abstract_packages[name];
        }
        std::vector<std::string> deps;
        for (const auto& dep : pair.second) {
            std::string resolved_dep = dep;
            if (abstract_packages.find(dep) != abstract_packages.end()) {
                resolved_dep = abstract_packages[dep];
            }
            deps.push_back(resolved_dep);
        }
        unified_adjacency_list[resolved_name] = deps;
    }

    std::vector<std::string> ordered_dependencies = topological_sort(unified_adjacency_list);
    std::reverse(ordered_dependencies.begin(), ordered_dependencies.end());

    return ordered_dependencies;
}

// Return: ((full list of dependencies, filtered list of dependencies), abstract packages)
std::pair<std::pair<std::vector<std::string>, std::vector<std::string>>, std::unordered_map<std::string, std::string>> resolve_dependencies(const std::string& pkg_name) {
    std::vector<std::string> ordered_dependencies = resolve_dependencies(pkg_name, false);
    std::vector<std::string> filtered_dependencies = resolve_dependencies(pkg_name, true);

    return { { ordered_dependencies, filtered_dependencies }, abstract_packages };
}