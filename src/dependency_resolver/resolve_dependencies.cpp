#include <algorithm>
#include <cstdlib>
#include <database/config.hpp>
#include <database/config_selector.hpp>
#include <database/database.hpp>
#include <dependency_resolver/advisor.hpp>
#include <dependency_resolver/optional_dependencies.hpp>
#include <dependency_resolver/requirements.hpp>
#include <dependency_resolver/resolve_dependencies.hpp>
#include <dependency_resolver/toposort.hpp>
#include <filesystem>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <utils/bash_utils.hpp>
#include <utils/colored_io.hpp>
#include <utils/file_utils.hpp>
#include <utils/string_utils.hpp>
#include <utils/terminal_ui.hpp>

namespace {
    struct OptionReference {
        std::string package;
        std::string option;
        std::vector<std::string> requirements;
    };

    /** @brief Mutable data accumulated during one dependency-resolution call. */
    struct ResolutionState {
        std::unordered_set<std::string> target_packages;
        bool interactive = false;
        std::filesystem::path database_root;
        DependencyGraph adjacency_list;
        std::unordered_set<std::string> system_packages;
        AbstractPackageSelections abstract_packages;
        std::vector<std::string> abstract_order;
        std::unordered_set<std::string> encountered_abstract_packages;
        std::unordered_map<std::string, bool> optional_package_choices;
        std::vector<std::string> optional_package_order;
        std::unordered_map<std::string, std::vector<OptionReference>> option_references;
        InteractiveOptionSelections option_selections;
        /** @brief Maps package name → resolved version string, populated during
         *         graph construction.  Contains only packages that had explicit
         *         version constraints in their parent's dependency list. */
        std::unordered_map<std::string, std::string> package_versions;
    };

    std::string resolve_abstract(const ResolutionState& state, const std::string& package_name) {
        const auto selected = state.abstract_packages.find(package_name);
        return selected == state.abstract_packages.end() ? package_name : selected->second;
    }

    void validate_implementation(const std::string& abstract_package, const PackageConfig& config,
                                 const std::string& implementation) {
        if (std::find(config.implementations.begin(), config.implementations.end(),
                      implementation) == config.implementations.end()) {
            ERROR("Dependency advice selected '" + implementation +
                  "', which does not implement '" + abstract_package + "'");
            exit(EXIT_FAILURE);
        }
    }

    std::string select_implementation(ResolutionState& state, const std::string& abstract_package,
                                      const PackageConfig& config) {
        state.encountered_abstract_packages.insert(abstract_package);
        const auto selected = state.abstract_packages.find(abstract_package);
        if (selected != state.abstract_packages.end()) {
            return selected->second;
        }

        const std::string implementation = advise(abstract_package);
        validate_implementation(abstract_package, config, implementation);
        state.abstract_packages.emplace(abstract_package, implementation);
        state.abstract_order.push_back(abstract_package);
        if (!state.interactive) {
            INFO("Selected implementation for '" + abstract_package + "': " + implementation);
        }
        return implementation;
    }

    void register_option(ResolutionState& state, const std::string& requirement,
                         const std::string& package, const BuildOption& option) {
        const bool default_enabled =
            !option.enabled.has_value() || option.enabled->default_value.value_or(false);
        state.option_selections[package].emplace(option.name, default_enabled);

        std::vector<OptionReference>& references = state.option_references[requirement];
        const auto duplicate                     = std::find_if(
            references.begin(), references.end(), [&](const OptionReference& reference) {
                return reference.package == package && reference.option == option.name;
            });
        if (duplicate == references.end()) {
            references.push_back({package, option.name, option.requires});
        } else {
            for (const std::string& option_requirement : option.requires) {
                append_unique(duplicate->requirements, option_requirement);
            }
        }
    }

    bool optional_dependency_available(const ResolutionState& state,
                                       const std::string& dependency) {
        validate_package_name(dependency);
        return fs_regular_file(state.database_root / dependency / "latest.yaml");
    }

    void register_options(ResolutionState& state, const std::string& package,
                          const BuildConfiguration& configuration) {
        for (const BuildOption& option : configuration.options) {
            if (!option.user_configurable) {
                continue;
            }
            for (const std::string& requirement : option.requires) {
                register_option(state, requirement, package, option);
            }
        }
    }

    void register_optional_requirements(ResolutionState& state, const PackageConfig& config,
                                        const std::vector<std::string>& optional_dependencies) {
        for (const std::string& dependency : optional_dependencies) {
            const auto is_dependency = [&](const Dependency& dep) {
                return dep.name == dependency;
            };
            if (std::find_if(config.dependencies.begin(), config.dependencies.end(),
                             is_dependency) != config.dependencies.end()) {
                continue;
            }
            if (state.optional_package_choices.emplace(dependency, false).second) {
                state.optional_package_order.push_back(dependency);
            }
        }
        if (!config.build.has_value()) {
            return;
        }
        if (config.build->configurations.has_value()) {
            register_options(state, config.name, *config.build->configurations);
        }
        for (const BuildStage& stage : config.build->stages) {
            if (stage.configurations.has_value()) {
                register_options(state, config.name, *stage.configurations);
            }
        }
    }

    std::vector<std::string> select_dependencies(ResolutionState& state,
                                                 const PackageConfig& config) {
        std::vector<std::string> dependencies;
        dependencies.reserve(config.dependencies.size());
        for (const Dependency& dep : config.dependencies) {
            dependencies.push_back(dep.name);
        }
        const std::vector<std::string> optional_dependencies = get_optional_dependencies(config);
        std::vector<std::string> available_dependencies;

        bool printed_heading     = false;
        const auto print_heading = [&]() {
            if (!printed_heading) {
                INFO("Optional dependencies for '" + config.name + "':");
                printed_heading = true;
            }
        };
        for (const std::string& dependency : optional_dependencies) {
            if (std::find(dependencies.begin(), dependencies.end(), dependency) !=
                dependencies.end()) {
                continue;
            }
            if (!optional_dependency_available(state, dependency)) {
                print_heading();
                INFO("- Skip unavailable optional dependency: " + dependency);
                continue;
            }
            available_dependencies.push_back(dependency);
        }

        if (state.interactive) {
            register_optional_requirements(state, config, available_dependencies);
        }
        for (const std::string& dependency : available_dependencies) {
            const bool include =
                state.interactive ? state.optional_package_choices.at(dependency) : true;
            if (!state.interactive) {
                print_heading();
                INFO("- Include optional dependency: " + dependency);
            }
            if (include) append_unique(dependencies, dependency);
        }
        return dependencies;
    }

    std::string resolve_version_for_dependency(const PackageConfig& config,
                                               const std::string& dep_name) {
        for (const Dependency& dep : config.dependencies) {
            if (dep.name == dep_name && !dep.constraints.empty()) {
                return resolve_dependency_version(dep_name, dep.constraints);
            }
        }
        return "latest";
    }

    void build_adjacency_list(ResolutionState& state, const std::string& package_name,
                              const std::string& version = "latest") {
        if (state.adjacency_list.find(package_name) != state.adjacency_list.end()) {
            // If we already visited under "latest" but now need a specific version,
            // still skip — the first visit handles all transitive deps correctly
            // since version constraints only affect the immediate parent's view.
            return;
        }

        PackageConfigPtr config      = get_db_config(package_name, version);
        std::string concrete_package = package_name;
        if (config->type == PackageType::Abstract) {
            concrete_package = select_implementation(state, package_name, *config);
            if (state.adjacency_list.find(concrete_package) != state.adjacency_list.end()) {
                return;
            }
            config = get_db_config(concrete_package);
        }

        // Record the version used for this package so callers (e.g.
        // gen_user_config) can generate config entries pointing at the
        // correct version instead of always using "latest".
        state.package_versions[concrete_package] = version;

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

        std::vector<std::string> dependencies  = select_dependencies(state, *config);
        state.adjacency_list[concrete_package] = dependencies;
        for (const std::string& dependency : dependencies) {
            const std::string dep_version = resolve_version_for_dependency(*config, dependency);
            build_adjacency_list(state, dependency, dep_version);
        }
    }

    void build_graph(ResolutionState& state, const std::vector<std::string>& package_names) {
        state.adjacency_list.clear();
        state.system_packages.clear();
        state.encountered_abstract_packages.clear();
        for (const std::string& package_name : package_names) {
            build_adjacency_list(state, package_name);
        }
    }

    DependencyGraph unify_adjacency_list(const ResolutionState& state) {
        DependencyGraph unified;
        for (const auto& [package, dependencies] : state.adjacency_list) {
            std::vector<std::string>& unified_dependencies =
                unified[resolve_abstract(state, package)];
            for (const std::string& dependency : dependencies) {
                append_unique(unified_dependencies, resolve_abstract(state, dependency));
            }
        }
        return unified;
    }

    void update_optional_package_choices(ResolutionState& state) {
        for (auto& choice : state.optional_package_choices) {
            choice.second = false;
        }
        for (const auto& group : state.option_references) {
            const std::vector<OptionReference>& references = group.second;
            for (const OptionReference& reference : references) {
                const auto package = state.option_selections.find(reference.package);
                if (package == state.option_selections.end()) {
                    continue;
                }
                const auto option = package->second.find(reference.option);
                if (option == package->second.end() || !option->second) {
                    continue;
                }
                for (const std::string& requirement : reference.requirements) {
                    const auto optional = state.optional_package_choices.find(requirement);
                    if (optional != state.optional_package_choices.end()) {
                        optional->second = true;
                    }
                }
            }
        }
    }

    void select_build_options(ResolutionState& state,
                              const std::vector<std::string>& package_names) {
        std::unordered_set<std::string> prompted;
        while (true) {
            build_graph(state, package_names);
            std::vector<std::string> unprompted;
            for (const std::string& dependency : state.optional_package_order) {
                if (prompted.find(dependency) == prompted.end()) {
                    unprompted.push_back(dependency);
                }
            }
            if (unprompted.empty()) {
                return;
            }

            for (const std::string& dependency : unprompted) {
                prompted.insert(dependency);
                const auto group = state.option_references.find(dependency);
                if (group == state.option_references.end()) {
                    continue;
                }

                std::vector<const OptionReference*> available;
                std::vector<std::string> labels;
                std::vector<bool> selected;
                for (const OptionReference& reference : group->second) {
                    available.push_back(&reference);
                    labels.push_back(reference.package + "." + reference.option);
                    selected.push_back(
                        state.option_selections[reference.package][reference.option]);
                }
                if (available.empty()) {
                    continue;
                }

                INFO("Optional package " + dependency + " is required by the following options:");
                selected = terminal_select_multiple("Do you want to enable the following options?",
                                                    labels, selected);
                for (std::size_t index = 0; index < available.size(); ++index) {
                    state.option_selections[available[index]->package][available[index]->option] =
                        selected[index];
                }
            }
            update_optional_package_choices(state);
        }
    }

    void select_abstract_packages(ResolutionState& state,
                                  const std::vector<std::string>& package_names) {
        std::size_t prompted = 0;
        while (true) {
            while (prompted < state.abstract_order.size()) {
                const std::string abstract_package = state.abstract_order[prompted++];
                const PackageConfigPtr config      = get_db_config(abstract_package);
                INFO("Abstract package " + abstract_package +
                     " has the following implementations:");
                const std::optional<std::size_t> selected = terminal_select_one(
                    "Do you want to use the following implementation?", config->implementations);
                if (selected.has_value()) {
                    state.abstract_packages[abstract_package] = config->implementations[*selected];
                } else {
                    state.abstract_packages[abstract_package] = advise(abstract_package);
                    validate_implementation(abstract_package, *config,
                                            state.abstract_packages[abstract_package]);
                }
                INFO("Selected implementation for '" + abstract_package +
                     "': " + state.abstract_packages[abstract_package]);
            }

            build_graph(state, package_names);
            if (prompted == state.abstract_order.size()) {
                return;
            }
        }
    }

    std::vector<std::string> filter_system_packages(
        const std::vector<std::string>& packages,
        const std::unordered_set<std::string>& system_packages) {
        std::vector<std::string> filtered;
        filtered.reserve(packages.size());
        for (const std::string& package : packages) {
            if (system_packages.find(package) == system_packages.end()) {
                filtered.push_back(package);
            }
        }
        return filtered;
    }
}  // namespace

DependencyResolution resolve_dependencies(const std::vector<std::string>& package_names,
                                          bool interactive) {
    return resolve_dependencies(package_names, interactive, nullptr);
}

DependencyResolution resolve_dependencies(const std::vector<std::string>& package_names,
                                          bool interactive,
                                          InteractiveOptionSelections* option_selections) {
    if (option_selections != nullptr) {
        option_selections->clear();
    }
    if (package_names.empty()) {
        return {};
    }

    ResolutionState state;
    state.target_packages.insert(package_names.begin(), package_names.end());
    state.interactive   = interactive;
    state.database_root = get_env_var("KEZ_DB");
    if (interactive) {
        select_build_options(state, package_names);
        select_abstract_packages(state, package_names);
    } else {
        build_graph(state, package_names);
    }

    for (auto selection = state.abstract_packages.begin();
         selection != state.abstract_packages.end();) {
        if (state.encountered_abstract_packages.find(selection->first) ==
            state.encountered_abstract_packages.end()) {
            selection = state.abstract_packages.erase(selection);
        } else {
            ++selection;
        }
    }

    std::vector<std::string> all_packages = topological_sort(unify_adjacency_list(state));
    std::reverse(all_packages.begin(), all_packages.end());
    std::vector<std::string> filtered_packages =
        filter_system_packages(all_packages, state.system_packages);
    if (option_selections != nullptr) {
        *option_selections = std::move(state.option_selections);
    }
    return DependencyResolution {std::move(all_packages), std::move(filtered_packages),
                                 std::move(state.abstract_packages),
                                 std::move(state.package_versions)};
}
