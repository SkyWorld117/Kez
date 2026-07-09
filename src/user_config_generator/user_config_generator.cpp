#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cstdlib>
#include <database/config.hpp>
#include <database/database.hpp>
#include <dependency_resolver/resolve_dependencies.hpp>
#include <filesystem>
#include <optional>
#include <string>
#include <ui/ui_utils.hpp>
#include <unordered_set>
#include <user_config_generator/config_transformer.hpp>
#include <user_config_generator/configurations_filter.hpp>
#include <user_config_generator/stages_filter.hpp>
#include <user_config_generator/user_config_generator.hpp>
#include <utils/bash_utils.hpp>
#include <utils/colored_io.hpp>
#include <utils/string_utils.hpp>
#include <utils/yaml_utils.hpp>
#include <vector>

namespace {

    /**
     * @brief Determine the default compiler family from the environment
     *        configuration.
     *
     * Reads the `KEZ_WORKDIR` environment variable and looks for a
     * `config.yaml` file inside that directory.  If the file exists and
     * contains a scalar `settings.default_compiler` key, its value is
     * returned (e.g. "gcc", "llvm", "oneapi").  Otherwise the function
     * returns the fallback string "system", meaning no user-configured
     * default compiler was found and the system compiler should be used.
     *
     * The lookup is deliberately non-fatal: every missing or malformed
     * input (unset `KEZ_WORKDIR`, missing file, missing key, non-map
     * `settings`, non-scalar value) silently degrades to "system".
     *
     * @return The configured default compiler family as a string, or
     *         "system" if none is configured.
     */
    std::string configured_default_compiler() {
        const std::string work_directory = get_env_var_noerr("KEZ_WORKDIR");
        if (work_directory.empty()) {
            return "system";
        }

        const std::filesystem::path path = std::filesystem::path(work_directory) / "config.yaml";
        if (!std::filesystem::is_regular_file(path)) {
            return "system";
        }

        const YAML::Node document = cached_yaml_load(path);
        if (!yaml_has(document, "settings")) {
            return "system";
        }
        const YAML::Node settings = document["settings"];
        if (!settings.IsMap() || !yaml_has(settings, "default_compiler")) {
            return "system";
        }
        const YAML::Node compiler = settings["default_compiler"];
        if (!compiler.IsScalar()) {
            return "system";
        }
        return yaml_scalar(compiler, "settings.default_compiler");
    }

    /**
     * @brief Resolve a list of target package names, substituting abstract
     *        package names with their concrete selections.
     *
     * Iterates over every entry in @p target_packages.  If an entry appears
     * as a key in @p abstract_packages, it is replaced by the corresponding
     * concrete implementation name (e.g. "blas" -> "mkl").  Entries that are
     * not found in @p abstract_packages are kept as-is.  The result is an
     * unordered set of concrete package names that can be used for fast
     * membership tests.
     *
     * @param target_packages   The original list of top-level package names
     *                          requested by the user.
     * @param abstract_packages Mapping from abstract package names to their
     *                          chosen concrete implementations, as produced
     *                          by resolve_dependencies().
     *
     * @return An unordered_set containing the resolved concrete names.  The
     *         set has the same cardinality as @p target_packages.
     */
    std::unordered_set<std::string> resolved_targets(
        const std::vector<std::string>& target_packages,
        const AbstractPackageSelections& abstract_packages) {
        std::unordered_set<std::string> result;
        result.reserve(target_packages.size());
        for (const std::string& target : target_packages) {
            const auto selected = abstract_packages.find(target);
            result.insert(selected == abstract_packages.end() ? target : selected->second);
        }
        return result;
    }

    /**
     * @brief List the available patch files for a given package.
     *
     * Scans the directory `$KEZ_HOME/patches/<package_name>/` for regular
     * files and returns their filenames in lexicographic order.  If
     * `KEZ_HOME` is unset or the patch directory does not exist, an empty
     * vector is returned silently.
     *
     * @param package_name  The name of the package whose patches should be
     *                      listed (e.g. "hdf5", "openmpi").
     *
     * @return A sorted vector of patch filenames (e.g. {"fix-pgi.patch",
     *         "shared.patch"}), or an empty vector if no patches are
     *         available.
     */
    std::vector<std::string> available_patches(const std::string& package_name) {
        const std::string home = get_env_var_noerr("KEZ_HOME");
        if (home.empty()) {
            return {};
        }

        const std::filesystem::path directory =
            std::filesystem::path(home) / "patches" / package_name;
        if (!std::filesystem::is_directory(directory)) {
            return {};
        }

        std::vector<std::string> result;
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                result.push_back(entry.path().filename().string());
            }
        }
        std::sort(result.begin(), result.end());
        return result;
    }

    /**
     * @brief Find the highest existing installed version of a package within
     *        a given work-path subdirectory.
     *
     * Scans the directory pointed to by `configured_work_path(path_name)`
     * for subdirectories whose name starts with `<package_name>-`.  The
     * remainder of the directory name after the prefix is treated as a
     * version string.  For MPI packages (`PackageType::Mpi`), the trailing
     * component after the last dash is stripped (it typically encodes the
     * compiler suffix) before version comparison.
     *
     * The highest version is determined by `compare_versions()` (numeric
     * segment-based comparison).  If no matching directory is found, an
     * empty string is returned.
     *
     * @param package_name  The name of the package to look for (e.g.
     *                      "openmpi", "cuda").
     * @param path_name     The environment-relative subdirectory to search
     *                      (e.g. "mpis", "vendors").
     * @param type          The package type, used to decide whether to strip
     *                      a trailing compiler-suffix segment from directory
     *                      names before comparing versions.  Only
     *                      `PackageType::Mpi` triggers this stripping.
     *
     * @return The highest version string found, or an empty string if no
     *         installed version exists.
     */
    std::string get_latest_existing_version(const std::string& package_name,
                                            const std::string& path_name, PackageType type) {
        const std::filesystem::path root = configured_work_path(path_name);
        if (!std::filesystem::is_directory(root)) {
            return {};
        }

        std::string latest_version;
        const std::string prefix = package_name + "-";

        for (auto it : std::filesystem::directory_iterator(root)) {
            if (!it.is_directory()) continue;
            std::string dirname = it.path().filename().string();
            if (dirname.rfind(prefix, 0) != 0) continue;
            std::string version = dirname.substr(prefix.size());
            if (type == PackageType::Mpi) {
                std::size_t last_dash = version.find_last_of('-');
                if (last_dash != std::string::npos) {
                    version = version.substr(0, last_dash);
                }
            }

            if (!version.empty()) {
                if (latest_version.empty() || compare_versions(version, latest_version) > 0) {
                    latest_version = version;
                }
            }
        }
        return latest_version;
    }

    /**
     * @brief Append a single package's YAML configuration block to the
     *        output node.
     *
     * Constructs the `kez/<package_name>` entry in @p output for the given
     * package.  The entry may contain the following keys depending on the
     * package's recipe and the resolution context:
     *
     *   - **description**  — The package's human-readable description
     *                        (if present in the database recipe).
     *   - **version**      — The selected version string, taken from the
     *                        first release in the package's source definition.
     *                        For MPI and Vendor packages with an already-
     *                        installed instance, the already-installed version
     *                        is preferred so that re-generation honours
     *                        existing installations.
     *   - **compiler**     — The default compiler family, omitted for Vendor
     *                        and External package types.
     *   - **patches**      — A sequence of available patch files, each
     *                        defaulting to `enabled: false`.  Omitted entirely
     *                        when no patches exist for the package.
     *   - **build**        — A map containing `configurations` and `stages`
     *                        produced by filtering and transforming the
     *                        package's Build recipe.  The build block is
     *                        included only when the package is not a Compiler
     *                        or MPI package (unless it is a user-requested
     *                        target) **and** its recipe defines a `build`
     *                        section **and** at least one configuration or
     *                        stage survives filtering.
     *
     * @param output               The YAML::Node (expected to be a Map under
     *                             the "kez" key) to which the package block
     *                             is appended.  Modified in-place.
     * @param package              The parsed PackageConfig for the current
     *                             dependency.
     * @param all_dependencies     The full list of resolved package names
     *                             (including system-level packages) in
     *                             dependent-before-dependency order.  Used
     *                             to evaluate dependency-based filters on
     *                             configurations and stages.
     * @param all_dependency_set   An unordered_set version of
     *                             @p all_dependencies for O(1) membership
     *                             lookups during build transformation.
     * @param target_packages      The set of concrete package names that
     *                             were requested as top-level targets.
     *                             Determines whether the build section is
     *                             emitted for compiler and MPI packages.
     * @param abstract_packages    Mapping from abstract package names to
     *                             their chosen concrete implementations.
     *                             Used to resolve abstract references in
     *                             build configurations and stage filters.
     * @param default_compiler     The compiler family to use as the default
     *                             for this package (e.g. "gcc", "llvm").
     */
    void append_package_config(YAML::Node& output, const PackageConfig& package,
                               const std::vector<std::string>& all_dependencies,
                               const std::unordered_set<std::string>& all_dependency_set,
                               const std::unordered_set<std::string>& target_packages,
                               const AbstractPackageSelections& abstract_packages,
                               const std::string& default_compiler) {
        YAML::Node package_output(YAML::NodeType::Map);
        if (package.description.has_value()) {
            package_output["description"] = *package.description;
        }
        if (package.source.has_value() && !package.source->releases.empty()) {
            std::string version = package.source->releases.front().version;
            if (package.type == PackageType::Mpi) {
                std::string existing =
                    get_latest_existing_version(package.name, "mpis", PackageType::Mpi);
                if (!existing.empty()) version = existing;
            } else if (package.type == PackageType::Vendor) {
                std::string existing =
                    get_latest_existing_version(package.name, "vendors", PackageType::Vendor);
                if (!existing.empty()) version = existing;
            }
            package_output["version"] = version;
        }
        if (package.type != PackageType::Vendor && package.type != PackageType::External) {
            package_output["compiler"] = default_compiler;
        }

        const std::vector<std::string> patches = available_patches(package.name);
        if (!patches.empty()) {
            YAML::Node patch_output(YAML::NodeType::Sequence);
            for (const std::string& patch : patches) {
                YAML::Node item(YAML::NodeType::Map);
                item["name"]    = patch;
                item["enabled"] = false;
                patch_output.push_back(item);
            }
            package_output["patches"] = patch_output;
        }

        const bool include_build =
            (package.type != PackageType::Compiler && package.type != PackageType::Mpi) ||
            target_packages.find(package.name) != target_packages.end();
        if (include_build && package.build.has_value()) {
            const std::optional<Build> build = user_config_generator::transformed_build(
                package, all_dependency_set, abstract_packages, default_compiler);
            YAML::Node build_output(YAML::NodeType::Map);
            if (build->configurations.has_value()) {
                YAML::Node configurations = filtered_configurations(
                    *build->configurations, all_dependencies, abstract_packages);
                if (configurations.size() != 0) {
                    build_output["configurations"] = configurations;
                }
            }

            YAML::Node stages = filtered_stages(build->stages, all_dependencies, abstract_packages);
            if (stages.size() != 0) {
                build_output["stages"] = stages;
            }
            if (build_output.size() != 0) {
                package_output["build"] = build_output;
            }
        }

        output["kez"][package.name] = package_output;
    }
}  // namespace

/**
 * @brief Generate a user-editable YAML configuration from a list of package
 *        names, using the default compiler configured in the environment.
 *
 * This is a convenience overload that reads the default compiler from the
 * environment's `config.yaml` (via configured_default_compiler()) and
 * delegates to the three-parameter overload of gen_user_config().  If no
 * default compiler is configured, "system" is used.
 *
 * @param package_names  Non-empty list of top-level package names to include
 *                       in the generated configuration.  Each name is matched
 *                       case-sensitively against the package database.
 * @param interactive    If true, the dependency resolver may prompt the user
 *                       interactively for abstract-package implementations and
 *                       optional-dependency decisions.  If false, heuristics
 *                       and sensible defaults are used instead.
 *
 * @return A YAML::Node containing the full abstract configuration tree,
 *         structured as a top-level map with `kez` and `recipe` sections.
 *
 * @warning Terminates the process via ERROR() if any package name is
 *          unknown, the dependency graph cannot be resolved, or no
 *          dependencies are found for the requested packages.
 *
 * @see gen_user_config(const std::vector<std::string>&, bool,
 *                      const std::string&)
 * @see configured_default_compiler()
 */
YAML::Node gen_user_config(const std::vector<std::string>& package_names, bool interactive) {
    return gen_user_config(package_names, interactive, configured_default_compiler());
}

/**
 * @brief Generate a user-editable YAML configuration from a list of package
 *        names with an explicit default compiler choice.
 *
 * This is the principal implementation of the user-config generator.  It
 * drives the following pipeline:
 *
 *   1. **Dependency resolution** — Calls resolve_dependencies() to obtain
 *      the full dependency graph (including transitive dependencies), the
 *      subset of non-system packages, and the mapping from abstract package
 *      names to their chosen concrete implementations.
 *
 *   2. **Early exit on empty result** — If the dependency resolver returned
 *      no non-system dependencies, the function prints a fatal error listing
 *      all input package names and terminates.
 *
 *   3. **Recipe section generation** — Populates `recipe/abstract_packages`
 *      (the abstract-to-concrete mapping, sorted alphabetically),
 *      `recipe/dependencies` (the full dependency list), and `recipe/targets`
 *      (the original target names).
 *
 *   4. **Per-package configuration** — For every non-system dependency (in
 *      the order returned by the resolver), calls append_package_config() to
 *      build its `kez/<name>` entry.  Each entry carries the package's
 *      description, selected version, compiler assignment, available patches,
 *      and filtered build configuration / stages.
 *
 * @param package_names    Non-empty list of top-level package names to
 *                         include in the generated configuration.
 * @param interactive      If true, the dependency resolver may prompt the
 *                         user interactively for choices.  If false,
 *                         heuristics and defaults are used.
 * @param default_compiler The compiler family to use as the default for
 *                         packages that support it (e.g. "gcc", "llvm",
 *                         "oneapi").  Pass "system" to indicate no user-
 *                         configured preference.
 *
 * @return A YAML::Node containing the full abstract configuration tree.
 *         The returned node is a mapping with two top-level keys:
 *         - `kez`    — Maps each package name to its configuration block.
 *         - `recipe` — Metadata about the resolution (abstract_packages,
 *                      dependencies, targets).
 *
 * @warning Terminates the process via ERROR() and exit(EXIT_FAILURE) if:
 *          - Any package name is not found in the database.
 *          - The dependency graph contains an unsatisfiable cycle.
 *          - An abstract package has no valid implementation for the
 *            target architecture (non-interactive mode).
 *          - The resolved dependency list is empty (no non-system
 *            dependencies found for any input package).
 *
 * @note The version for MPI and Vendor packages is overridden by the
 *       highest already-installed version in the work path, so that
 *       re-generating a config for an existing environment does not
 *       inadvertently suggest a downgrade.
 */
YAML::Node gen_user_config(const std::vector<std::string>& package_names, bool interactive,
                           const std::string& default_compiler) {
    DependencyResolution resolution = resolve_dependencies(package_names, interactive);
    const std::vector<std::string>& all_dependencies   = resolution.first.first;
    const std::vector<std::string>& dependencies       = resolution.first.second;
    const AbstractPackageSelections& abstract_packages = resolution.second;

    if (dependencies.empty()) {
        std::string packages;
        for (const std::string& package : package_names) {
            packages += (packages.empty() ? "" : " ") + package;
        }
        ERROR("No dependencies found for packages: " + packages);
        exit(EXIT_FAILURE);
    }

    YAML::Node output(YAML::NodeType::Map);
    output["kez"]    = YAML::Node(YAML::NodeType::Map);
    output["recipe"] = YAML::Node(YAML::NodeType::Map);

    output["recipe"]["abstract_packages"] = YAML::Node(YAML::NodeType::Map);
    std::vector<std::string> abstract_names;
    abstract_names.reserve(abstract_packages.size());
    for (const auto& selection : abstract_packages) {
        abstract_names.push_back(selection.first);
    }
    std::sort(abstract_names.begin(), abstract_names.end());
    for (const std::string& abstract_package : abstract_names) {
        output["recipe"]["abstract_packages"][abstract_package] =
            abstract_packages.at(abstract_package);
    }

    output["recipe"]["dependencies"] = all_dependencies;
    output["recipe"]["targets"]      = package_names;

    const std::unordered_set<std::string> target_packages =
        resolved_targets(package_names, abstract_packages);
    const std::unordered_set<std::string> all_dependency_set(all_dependencies.begin(),
                                                             all_dependencies.end());
    for (const std::string& dependency : dependencies) {
        const PackageConfigPtr package = get_db_config(dependency);
        append_package_config(output, *package, all_dependencies, all_dependency_set,
                              target_packages, abstract_packages, default_compiler);
    }
    return output;
}
