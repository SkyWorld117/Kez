#include <algorithm>
#include <cstdlib>
#include <database/config.hpp>
#include <database/database.hpp>
#include <dependency_resolver/advisor.hpp>
#include <dependency_resolver/essential_dependencies.hpp>
#include <dependency_resolver/optional_dependencies.hpp>
#include <dependency_resolver/resolve_dependencies.hpp>
#include <dependency_resolver/toposort.hpp>
#include <iostream>
#include <unordered_set>
#include <utils/colored_io.hpp>
#include <utils/string_utils.hpp>

namespace {
    struct ResolutionState {
        std::unordered_set<std::string> target_packages;
        bool interactive = false;
        DependencyGraph adjacency_list;
        std::unordered_set<std::string> system_packages;
        AbstractPackageSelections abstract_packages;
        std::unordered_map<std::string, bool> optional_package_choices;
    };

    std::string resolve_abstract(const ResolutionState& state, const std::string& package_name) {
        const auto selected = state.abstract_packages.find(package_name);
        return selected == state.abstract_packages.end() ? package_name : selected->second;
    }

    std::string select_implementation(ResolutionState& state, const std::string& abstract_package,
                                      const PackageConfig& config) {
        const auto selected = state.abstract_packages.find(abstract_package);
        if (selected != state.abstract_packages.end()) {
            return selected->second;
        }

        std::string implementation;
        if (state.interactive) {
            INFO("Package '" + abstract_package +
                 "' is abstract. Please select one of the following implementations:");
            for (const std::string& candidate : config.implementations) {
                INFO("- " + candidate);
            }
            while (true) {
                INFO("Enter the implementation name: ");
                std::string input;
                if (!std::getline(std::cin, input)) {
                    ERROR("Input ended while selecting an implementation for '" + abstract_package +
                          "'");
                    exit(EXIT_FAILURE);
                }
                implementation = trim(input);
                if (std::find(config.implementations.begin(), config.implementations.end(),
                              implementation) != config.implementations.end()) {
                    break;
                }
                ERROR("Invalid implementation selected: " + implementation);
            }
        } else {
            implementation = advise(abstract_package);
            INFO("Selected implementation for '" + abstract_package + "': " + implementation);
            if (std::find(config.implementations.begin(), config.implementations.end(),
                          implementation) == config.implementations.end()) {
                ERROR("Dependency advice selected '" + implementation +
                      "', which does not implement '" + abstract_package + "'");
                exit(EXIT_FAILURE);
            }
        }

        state.abstract_packages.emplace(abstract_package, implementation);
        return implementation;
    }

    std::vector<std::string> select_dependencies(ResolutionState& state,
                                                 const std::string& package_name) {
        std::vector<std::string> dependencies = get_essential_dependencies(package_name);
        const std::vector<std::string> optional_dependencies =
            get_optional_dependencies(package_name);
        bool printed_heading = false;

        for (const std::string& dependency : optional_dependencies) {
            if (std::find(dependencies.begin(), dependencies.end(), dependency) !=
                dependencies.end()) {
                continue;
            }

            auto choice = state.optional_package_choices.find(dependency);
            if (choice == state.optional_package_choices.end()) {
                if (!printed_heading) {
                    INFO("Optional dependencies for '" + package_name + "':");
                    printed_heading = true;
                }
                bool include = false;
                if (state.interactive) {
                    INFO("- Include optional dependency: " + dependency + "? (y/n)");
                    std::string input;
                    if (!std::getline(std::cin, input)) {
                        ERROR("Input ended while selecting optional dependency '" + dependency +
                              "'");
                        exit(EXIT_FAILURE);
                    }
                    input   = trim(input);
                    include = input == "y" || input == "Y";
                } else {
                    INFO("- Exclude optional dependency: " + dependency);
                }
                choice = state.optional_package_choices.emplace(dependency, include).first;
            }
            if (choice->second) {
                append_unique(dependencies, dependency);
            }
        }
        return dependencies;
    }

    void build_adjacency_list(ResolutionState& state, const std::string& package_name) {
        if (state.adjacency_list.find(package_name) != state.adjacency_list.end()) {
            return;
        }

        PackageConfigPtr config      = get_db_config(package_name);
        std::string concrete_package = package_name;
        if (config->type == PackageType::Abstract) {
            concrete_package = select_implementation(state, package_name, *config);
            if (state.adjacency_list.find(concrete_package) != state.adjacency_list.end()) {
                return;
            }
            config = get_db_config(concrete_package);
        }

        if (config->type == PackageType::System) {
            state.adjacency_list[concrete_package] = {};
            state.system_packages.insert(concrete_package);
            return;
        }
        if ((config->type == PackageType::Compiler || config->type == PackageType::Mpi) &&
            state.target_packages.find(concrete_package) == state.target_packages.end()) {
            state.adjacency_list[concrete_package] = {};
            return;
        }

        std::vector<std::string> dependencies  = select_dependencies(state, concrete_package);
        state.adjacency_list[concrete_package] = dependencies;
        for (const std::string& dependency : dependencies) {
            build_adjacency_list(state, dependency);
        }
    }

    DependencyGraph unify_adjacency_list(const ResolutionState& state) {
        DependencyGraph unified_adjacency_list;
        for (const auto& [package, dependencies] : state.adjacency_list) {
            std::vector<std::string>& unified_dependencies =
                unified_adjacency_list[resolve_abstract(state, package)];
            for (const std::string& dependency : dependencies) {
                append_unique(unified_dependencies, resolve_abstract(state, dependency));
            }
        }
        return unified_adjacency_list;
    }

    std::vector<std::string> filter_system_packages(
        const std::vector<std::string>& packages,
        const std::unordered_set<std::string>& system_packages) {
        std::vector<std::string> filtered_packages;
        filtered_packages.reserve(packages.size());
        for (const std::string& package : packages) {
            if (system_packages.find(package) == system_packages.end()) {
                filtered_packages.push_back(package);
            }
        }
        return filtered_packages;
    }
}  // namespace

DependencyResolution resolve_dependencies(const std::vector<std::string>& package_names,
                                          bool interactive) {
    if (package_names.empty()) {
        return {};
    }

    ResolutionState state;
    state.target_packages.insert(package_names.begin(), package_names.end());
    state.interactive = interactive;
    for (const std::string& package_name : package_names) {
        build_adjacency_list(state, package_name);
    }

    std::vector<std::string> all_packages = topological_sort(unify_adjacency_list(state));
    std::reverse(all_packages.begin(), all_packages.end());
    std::vector<std::string> filtered_packages =
        filter_system_packages(all_packages, state.system_packages);
    return {{std::move(all_packages), std::move(filtered_packages)},
            std::move(state.abstract_packages)};
}
