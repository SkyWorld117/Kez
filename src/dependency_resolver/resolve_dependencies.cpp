#include "resolve_dependencies.h"

std::filesystem::path db_path(getenv("CHEESE_DB"));
std::unordered_map<std::string, std::vector<std::string>> adjacency_list;
std::unordered_map<std::string, bool> system_packages;
std::unordered_map<std::string, std::string> abstract_packages;
std::unordered_map<std::string, bool> use_optional_packages;

void build_adjacency_list(const std::string& pkg_name) {
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
            INFO("Enter the implementation name: ");
            std::getline(std::cin, selected_impl);
            // Remove leading/trailing whitespace/newline etc.
            selected_impl.erase(0, selected_impl.find_first_not_of(" \n\r\t"));
            selected_impl.erase(selected_impl.find_last_not_of(" \n\r\t") + 1);
            // Validate the selected implementation
            std::vector<std::string> implementations = config["cheese"]["implementations"].as<std::vector<std::string>>();
            if (std::find(implementations.begin(), implementations.end(), selected_impl) == implementations.end()) {
                ERROR("Invalid implementation selected: " + selected_impl);
                exit(EXIT_FAILURE);
            }
            abstract_packages[pkg_name] = selected_impl;
            concrete_pkg_name = selected_impl; // Use the selected implementation for further processing
        } else {
            concrete_pkg_name = abstract_packages[pkg_name]; // Use the already selected implementation
        }

        if (adjacency_list.find(concrete_pkg_name) != adjacency_list.end()) {
            return; // Already processed
        }
    } else {
        concrete_pkg_name = pkg_name; // Use the original package name for non-abstract packages
    }

    if (system_packages.find(concrete_pkg_name) == system_packages.end()) {
        system_packages[concrete_pkg_name] = config["cheese"]["type"].as<std::string>() == "system";
    }

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
        build_adjacency_list(dep);
    }
}

std::vector<std::string> resolve_dependencies(const std::string& pkg_name) {
    build_adjacency_list(pkg_name);

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

    // Perform topological sort on the unified adjacency list
    std::vector<std::string> ordered_dependencies = topological_sort(unified_adjacency_list);
    std::reverse(ordered_dependencies.begin(), ordered_dependencies.end());

    return ordered_dependencies;
}

std::pair<std::vector<std::string>, std::unordered_map<std::string, std::string>> resolve_dependencies(const std::string& pkg_name, bool filter_system_pkg) {
    std::vector<std::string> ordered_dependencies = resolve_dependencies(pkg_name);

    std::vector<std::string> filtered_dependencies;
    for (const auto& dep : ordered_dependencies) {
        if (!filter_system_pkg || !system_packages[dep]) {
            filtered_dependencies.push_back(dep);
        }
    }

    return { filtered_dependencies, abstract_packages };
}