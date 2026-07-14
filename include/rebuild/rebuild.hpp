#pragma once

#include <filesystem>
#include <string>
#include <uconf_parser/user_config_parser.hpp>
#include <unordered_map>
#include <vector>

/**
 * @brief Read the `state:` sequence from an environment's state.yaml into a
 *        list of installed package names.
 *
 * Opens `<env_prefix>/state.yaml`, locates the top-level `state` key, and
 * returns each scalar element as a package name.  The file is expected to
 * contain a YAML mapping with a `state` key whose value is a sequence of
 * strings, e.g.:
 * @code{.yaml}
 * state:
 *   - gcc
 *   - openmpi
 * @endcode
 *
 * @param env_prefix  Absolute path to the installed environment directory that
 *                    contains the state.yaml file.
 *
 * @return A vector of installed package name strings.  Returns an empty vector
 *         when `state.yaml` does not exist, when the `state` key is absent, or
 *         when `state:` is present but has no entries (parses as YAML null).
 *
 * @warning Terminates the program with a non-zero exit code if the file exists
 *          but cannot be parsed, if the `state` value is not a sequence, or if
 *          any entry is not a scalar string.
 */
std::vector<std::string> load_installed_packages(const std::filesystem::path& env_prefix);

/**
 * @brief Invert a BashCommandPlan's per-package dependency edges into a
 *        dependents map.
 *
 * For every package in the plan, each entry in its `.dependencies` vector is
 * used as a key in the result; the package's own name is appended to the
 * vector of packages that depend on that key.  The result maps
 * `dependency_name -> [package_1, package_2, ...]`.
 *
 * Dependency names are already concrete (abstract-to-concrete resolution
 * completed by the user-config parser) and filtered to buildable packages, so
 * the inversion yields a correct, actionable dependents graph.
 *
 * @param plan  The parsed build plan whose per-package `.dependencies` lists
 *              are inverted.  Must not be modified concurrently.
 *
 * @return An unordered map where each key is a package name that appears as a
 *         dependency, and the value is a vector of packages that directly
 *         depend on it.  Packages with no dependents are absent from the map.
 *
 * @see compute_rebuild_set()  Uses this map to traverse the transitive
 *                             closure of dependents.
 * @see append_unique()        Used internally to avoid duplicate entries.
 */
std::unordered_map<std::string, std::vector<std::string>> build_dependents_map(
    const BashCommandPlan& plan);

/**
 * @brief Compute the transitive closure of dependents for a target package.
 *
 * Starting from `target`, performs a breadth-first traversal of the plan's
 * dependency graph (inverted via build_dependents_map()) to collect every
 * package that directly or transitively depends on the target.  The target
 * itself is always included in the result.
 *
 * The result is emitted in plan order (the order packages appear in `plan`).
 * Because the user-config parser emits packages such that dependents precede
 * the packages they depend on, the returned vector will naturally list e.g.
 * high-level applications before the libraries they use.
 *
 * @param plan    The full build plan that contains both the target and all
 *                potential dependents.  Used both to build the dependents map
 *                and to preserve plan order in the output.
 * @param target  The package whose transitive dependent set is requested.
 *                Must match a `.package` field in one of the plan's entries.
 *
 * @return A vector of package names (including `target`) that must be rebuilt
 *         when `target` changes, ordered as they appear in `plan`.
 *
 * @see build_dependents_map()  Performs the graph inversion used internally.
 * @see filter_plan()           Can consume the return value to produce a
 *                              pruned BashCommandPlan.
 */
std::vector<std::string> compute_rebuild_set(const BashCommandPlan& plan,
                                             const std::string& target);

/**
 * @brief Keep only the PackageCommands whose package is in the specified set,
 *        preserving the original plan order.
 *
 * Iterates over `plan` and copies each entry whose `.package` field appears
 * in `keep` into a new plan.  Dependency edges are copied unchanged; the
 * install.sh script gracefully ignores edges that point at packages absent
 * from the filtered plan, so no edge-rewriting is necessary.
 *
 * @param plan  The input build plan to filter.  Not modified.
 * @param keep  The set of package names (as a vector) to retain.  Duplicates
 *              in this vector are harmless; internally a hash set is built for
 *              O(1) lookups.
 *
 * @return A new BashCommandPlan containing only the entries whose package
 *         name appears in `keep`, in the same relative order as `plan`.
 *
 * @see compute_rebuild_set()  The typical producer of the `keep` argument.
 */
BashCommandPlan filter_plan(const BashCommandPlan& plan, const std::vector<std::string>& keep);
