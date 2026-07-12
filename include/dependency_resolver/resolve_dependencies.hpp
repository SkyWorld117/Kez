#pragma once

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

/**
 * @brief A pair of package-name vectors representing the resolved dependency lists.
 *
 * The first element contains **all** resolved packages (including system-level
 * packages such as those of type PackageType::System, PackageType::Compiler, or
 * PackageType::Mpi) in the legacy resolver's dependent-before-dependency order.
 * The second element contains only the non-system subset of those packages,
 * preserving the same relative ordering.  Both vectors are produced by the
 * resolve_dependencies() function.
 *
 * @see DependencyResolution
 * @see resolve_dependencies()
 */
using DependencyLists = std::pair<std::vector<std::string>, std::vector<std::string>>;

/**
 * @brief A mapping from abstract package names to their selected concrete
 *        implementations.
 *
 * Each key is the name of an abstract package (e.g. "blas", "lapack", "mpi"),
 * and the corresponding value is the concrete implementation chosen for it
 * during dependency resolution (e.g. "mkl", "openblas", "openmpi").  An entry
 * exists only for abstract packages that were actually encountered while
 * traversing the dependency graph.
 *
 * Selections are made either interactively from a mutually-exclusive list or
 * automatically via the heuristics advisor. In interactive mode, selecting no
 * implementation also uses the advisor default.
 *
 * @see resolve_dependencies()
 * @see advise()
 */
using AbstractPackageSelections = std::unordered_map<std::string, std::string>;

/**
 * @brief Enabled-state overrides chosen by the interactive user-config UI.
 *
 * The outer key is a concrete package name and the inner key is a build-option
 * name.  Every option shown by the interactive checklist is recorded, including
 * unchecked options, so generated YAML can override both true and false recipe
 * defaults consistently.
 */
using InteractiveOptionSelections =
    std::unordered_map<std::string, std::unordered_map<std::string, bool>>;

/**
 * @brief The complete result of dependency resolution for one or more target
 *        packages.
 *
 * A pair whose:
 *   - **first** member is a DependencyLists containing the ordered, resolved
 *     package lists (all packages, then non-system packages only).
 *   - **second** member is an AbstractPackageSelections mapping every abstract
 *     package encountered to the concrete implementation that was chosen for it.
 *
 * @see resolve_dependencies()
 */
using DependencyResolution = std::pair<DependencyLists, AbstractPackageSelections>;

/**
 * @brief Resolve the full dependency graph for one or more target packages.
 *
 * Starting from the given target packages, this function drives the core
 * resolution pipeline:
 *
 *   1. **Adjacency-list construction** -- For each target and every package
 *      reachable from it, the function retrieves the package's database
 *      configuration and collects its essential (hard) dependencies via
 *      get_essential_dependencies().  Optional (conditional) dependencies are
 *      handled according to the @p interactive flag (see below). Interactive
 *      discovery groups configurable options by the optional package they
 *      require before asking for option states.
 *
 *   2. **Abstract-package resolution** -- If a package's type is
 *      PackageType::Abstract, a concrete implementation must be selected.
 *      In interactive mode the implementation selector is shown after optional
 *      packages and their build options have been processed. In
 *      non-interactive mode the heuristics advisor (advise()) is consulted
 *      directly. The chosen implementation is then resolved in the same
 *      manner. Abstract packages that are
 *      **not** among the initial targets and whose type is PackageType::System,
 *      PackageType::Compiler, or PackageType::Mpi are treated as leaf nodes
 *      (their transitive dependencies are **not** traversed).
 *
 *   3. **Topological sort** -- The resulting concrete adjacency list is
 *      topologically sorted via topological_sort() and then **reversed** so
 *      that dependents appear before their dependencies (a "dependent-before-
 *      dependency" order).
 *
 *   4. **System-package filtering** -- Packages whose type is
 *      PackageType::System are identified and a second list containing only
 *      non-system packages is produced, preserving the original order.
 *
 * @param package_names  A vector of package names whose dependency graphs
 *                       should be resolved.  Each name must correspond to a
 *                       valid package recipe in the database (by name at the
 *                       "latest" version).  If the vector is empty, a default-
 *                       constructed DependencyResolution is returned (both
 *                       vectors and the map are empty).
 * @param interactive    Controls how abstract-package implementations and
 *                       optional dependencies are selected:
 *                       - **true**  -- The function reads from stdin. It
 *                           presents one grouped build-option checklist per
 *                           optional package and includes that package whenever
 *                           at least one requiring option is enabled. It then
 *                           presents mutually-exclusive abstract implementation
 *                           lists. An empty implementation selection uses the
 *                           advisor default.
 *                       - **false** -- Implementations are chosen automatically
 *                           by calling the heuristics advisor (advise()).
 *                           Optional dependencies are excluded and an INFO
 *                           message is printed for each excluded dependency.
 *
 * @return A DependencyResolution containing:
 *         - **first.first**  -- All resolved packages (including system-level
 *           packages) in dependent-before-dependency order.
 *         - **first.second** -- The subset of non-system packages from the
 *           above list, preserving the same relative ordering.
 *         - **second**       -- The map of abstract package name to chosen
 *           concrete implementation for every abstract package encountered.
 *
 * @note **Ordering semantics.**  The returned package lists use the legacy
 *       resolver's "dependent-before-dependency" convention: a package that
 *       depends on others appears earlier in the list than its dependencies.
 *       This is the reverse of a standard topological order and matches the
 *       ordering expected by downstream consumers of the resolution result.
 *
 * @note **Leaf-node shortcut.**  Compiler and MPI packages that are not in the
 *       original set of targets are inserted as leaf nodes: their adjacency-
 *       list entry is set to an empty vector and their transitive dependencies
 *       are not explored.  This prevents the resolver from descending into
 *       platform-level toolchains that are assumed to be pre-installed.
 *
 * @warning The function **terminates the program** (via the ERROR macro and
 *          exit(EXIT_FAILURE)) if:
 *          - a package name does not exist in the database,
 *          - an abstract package has no valid implementation according to the
 *            heuristics advisor (non-interactive mode),
 *          - the chosen concrete package itself cannot be found in the database,
 *          - stdin input fails (EOF or read error) during interactive mode.
 *
 * @see get_essential_dependencies()   Collects hard dependencies of a package.
 * @see get_optional_dependencies()    Collects conditional dependencies.
 * @see topological_sort()             Produces the initial topological order.
 * @see advise()                       Heuristics-based implementation picker.
 * @see PackageType                    Enum whose values drive traversal policy
 *                                     (System, Compiler, Mpi, Abstract, ...).
 * @see get_db_config()                Database lookup backing the resolution.
 * @see DependencyLists                Return-type component for the package
 *                                     vectors.
 * @see AbstractPackageSelections      Return-type component for the abstract
 *                                     resolution map.
 */
DependencyResolution resolve_dependencies(const std::vector<std::string>& package_names,
                                          bool interactive);

/**
 * @brief Resolve dependencies and return interactive build-option decisions.
 *
 * This overload is intended for user-configuration generation.  Its dependency
 * result is identical to the two-argument overload, while @p option_selections
 * receives the option states chosen after optional-package discovery.  The map
 * is cleared before resolution.  In non-interactive mode it remains empty.
 *
 * @param package_names       Top-level package names to resolve.
 * @param interactive         Whether to run the terminal selection workflow.
 * @param option_selections   Output map for package/option enabled states. May
 *                            be @c nullptr when the caller does not need them.
 */
DependencyResolution resolve_dependencies(const std::vector<std::string>& package_names,
                                          bool interactive,
                                          InteractiveOptionSelections* option_selections);
