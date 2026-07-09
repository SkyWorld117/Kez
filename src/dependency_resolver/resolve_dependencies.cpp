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
    /**
     * @brief State that accumulates during a dependency-resolution pass.
     *
     * Carries the target package set, the user-interactive flag, the
     * adjacency list being built, the set of system-type packages
     * encountered, the mapping from abstract package names to concrete
     * implementations that have been chosen so far, and the decisions
     * already made about optional dependencies.
     */
    struct ResolutionState {
        std::unordered_set<std::string> target_packages;
        bool interactive = false;
        DependencyGraph adjacency_list;
        std::unordered_set<std::string> system_packages;
        AbstractPackageSelections abstract_packages;
        std::unordered_map<std::string, bool> optional_package_choices;
    };

    /**
     * @brief Resolve an abstract package name to its concrete implementation.
     *
     * Looks up @p package_name in the in-progress selections. If a choice has
     * already been recorded (via a previous call to select_implementation),
     * that concrete name is returned immediately; otherwise the abstract name
     * itself is returned unchanged.
     *
     * @param state       Current resolution state holding the selection map.
     * @param package_name  The package name to resolve (may be abstract).
     * @return The concrete implementation name if one has been chosen,
     *         otherwise @p package_name unchanged.
     */
    std::string resolve_abstract(const ResolutionState& state, const std::string& package_name) {
        const auto selected = state.abstract_packages.find(package_name);
        return selected == state.abstract_packages.end() ? package_name : selected->second;
    }

    /**
     * @brief Choose (or retrieve) a concrete implementation for an abstract package.
     *
     * If @p abstract_package already has a selection recorded in @p state it is
     * returned immediately.  Otherwise the function enters either interactive or
     * automatic selection mode:
     *
     *   - **Interactive** (`state.interactive == true`): prints the list of
     *     valid implementations and loops reading stdin until a valid choice
     *     is entered.  If stdin ends (EOF/error) the program terminates via
     *     `ERROR()` + `exit(EXIT_FAILURE)`.
     *
     *   - **Automatic** (`state.interactive == false`): delegates to `advise()`
     *     (see advisor.hpp).  If the advice returns a value that is not among
     *     the declared implementations, the program terminates via `ERROR()` +
     *     `exit(EXIT_FAILURE)`.
     *
     * The chosen implementation is recorded in `state.abstract_packages` for
     * subsequent calls to `resolve_abstract()`.
     *
     * @param state              Resolution state (the selection map is
     *                           updated as a side effect).
     * @param abstract_package   Name of the abstract package to resolve.
     * @param config             The loaded database config for the abstract
     *                           package (provides the list of valid
     *                           implementations).
     * @return The selected concrete implementation name.
     */
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

    /**
     * @brief Collect the full set of dependencies for a package, including
     *        user-decided optional ones.
     *
     * Starts from the essential (mandatory) dependencies obtained via
     * `get_essential_dependencies()`.  For each optional dependency returned
     * by `get_optional_dependencies()` that is not already in the essential
     * set, a decision is made:
     *
     *   - If the optional dependency has already been decided in
     *     `state.optional_package_choices`, that decision is reused.
     *   - In interactive mode the user is prompted (y/n); if input fails the
     *     program terminates via `ERROR()` + `exit(EXIT_FAILURE)`.
     *   - In non-interactive mode the optional dependency is **excluded**
     *     (default: skip).
     *
     * Only those optional dependencies that were positively selected are
     * appended to the returned list.
     *
     * @param state        Resolution state (optional-package choices are
     *                     updated as a side effect, and reused across
     *                     subsequent calls).
     * @param package_name   The concrete package name whose dependencies
     *                       are being selected.
     * @return A vector containing all essential dependencies plus any
     *         optional dependencies the user (or default) chose to include.
     */
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

    /**
     * @brief Recursively build the dependency adjacency list for a package
     *        and all of its transitive dependencies (depth-first traversal).
     *
     * The logic proceeds as follows:
     *
     *   1. If @p package_name is already present in the adjacency list the
     *      call returns immediately (cycle / shared-dependency guard).
     *   2. The database config for @p package_name is loaded.
     *   3. If the config indicates an **abstract** package, a concrete
     *      implementation is chosen (via `select_implementation`).  If that
     *      concrete package is already in the adjacency list we return;
     *      otherwise we reload the config for the concrete package.
     *   4. **System** packages are recorded with an empty dependency vector
     *      and added to `state.system_packages`; no further recursion.
     *   5. **Compiler** and **Mpi** packages that are *not* in the user's
     *      explicit target set are treated as leaf nodes (empty dependencies,
     *      no further recursion) — they are assumed to be pre-installed
     *      system components.
     *   6. For all other packages, dependencies are selected via
     *      `select_dependencies`, stored in the adjacency list, and each
     *      dependency is recursively processed.
     *
     * @param state        Resolution state (adjacency list, system packages,
     *                     abstract selections, and optional choices are all
     *                     updated as side effects).
     * @param package_name   The package (or abstract package) to build from.
     */
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

    /**
     * @brief Resolve abstract names in the adjacency list to their concrete
     *        implementations, producing a unified graph.
     *
     * Iterates over every entry in the internal adjacency list.  For each
     * node and each of its dependencies, the abstract name (if any) is
     * replaced with the concrete name already chosen and stored in
     * `resolve_abstract()`.  Duplicate dependency entries within a single
     * node's list are eliminated via `append_unique()`.
     *
     * The returned graph is suitable for topological sorting.
     *
     * @param state   The resolution state whose adjacency list and abstract
     *                selections are used.
     * @return A new adjacency list with all abstract names concretised.
     */
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

    /**
     * @brief Remove all system-type packages from an ordered package list.
     *
     * Scans @p packages and copies every entry that is not present in
     * @p system_packages into a new vector, preserving the original order.
     *
     * @param packages          The ordered list of all packages (e.g. the
     *                          topological-sort output).
     * @param system_packages   The set of packages that were identified as
     *                          system-type during graph building.
     * @return A new vector containing only non-system packages, in the
     *         original relative order.
     */
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

/**
 * @brief Compute a full dependency resolution for a set of target packages.
 *
 * This is the top-level entry point of the dependency-resolution pipeline.
 * It performs the following steps:
 *
 *   1. Returns an empty result immediately if @p package_names is empty.
 *   2. Initialises a `ResolutionState` seeded with the target packages and
 *      the interactivity flag.
 *   3. For every target package, invokes `build_adjacency_list()` to
 *      recursively construct the full transitive dependency graph.
 *      During this phase abstract packages are resolved, system packages
 *      are identified, and optional-dependency decisions are collected
 *      (interactively or via heuristics/advice).
 *   4. Calls `unify_adjacency_list()` to replace any remaining abstract
 *      names with their concrete choices.
 *   5. Topologically sorts the unified graph (via `topological_sort`),
 *      then **reverses** the result so that the least-dependent packages
 *      (install-first) appear first.
 *   6. Removes all system-type packages from the reversed order via
 *      `filter_system_packages()`.
 *
 * The return value carries both the full ordered list (including system
 * packages) and the filtered list (user-installable packages only), as well
 * as the abstract-to-concrete mapping that was resolved.
 *
 * The program terminates via `ERROR()` + `exit(EXIT_FAILURE)` if:
 *   - The interactive selection loop encounters EOF or an I/O error on
 *     stdin (see `select_implementation` and `select_dependencies`).
 *   - The heuristics advisor selects an implementation that does not
 *     appear in the package's declared `implementations` list
 *     (see `select_implementation`).
 *
 * @param package_names   The set of packages the user wants to install
 *                        (may be empty).
 * @param interactive     If true the user is prompted interactively for
 *                        abstract-package implementations and optional
 *                        dependency choices.  If false, the heuristics
 *                        advisor is used and optional dependencies are
 *                        excluded by default.
 * @return A `DependencyResolution` containing:
 *         - `all_packages`   — full topological-sort order (reversed,
 *                              system packages included).
 *         - `filtered_packages` — only non-system packages, in install
 *                                 order.
 *         - `abstract_packages` — mapping of abstract -> concrete names
 *                                 that was resolved.
 *         Returns an empty `DependencyResolution` when the input list is
 *         empty.
 */
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
