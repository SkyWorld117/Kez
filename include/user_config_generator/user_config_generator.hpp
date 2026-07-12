#pragma once

#include <yaml-cpp/yaml.h>

#include <string>
#include <vector>

/**
 * @brief Generate a user-editable YAML configuration from a list of package names.
 *
 * This is the primary entry point for producing the abstract configuration that
 * the user can review and modify before installation. It starts from the given
 * package names, resolves their full dependency graph (including transitive
 * dependencies and architecture-appropriate substitutions via heuristics), and
 * emits a YAML::Node structured as follows:
 *
 * @code{.yaml}
 * kez:
 *   <package-name>:
 *     description: ...
 *     version: ...
 *     compiler: ...
 *     patches:
 *       - name: ...
 *         enabled: false
 *     build:
 *       configurations:
 *         environment: [...]
 *         options: [...]
 *       stages:
 *         - target: ...
 *           configurations: {...}
 * recipe:
 *   abstract_packages:
 *     <abstract-name>: <concrete-name>
 *   dependencies: [...]
 *   targets: [...]
 * @endcode
 *
 * The `kez` map contains per-package settings that the user can customise.
 * The `recipe` section records the resolved dependency graph and abstract-package
 * selections.
 *
 * @param package_names  Non-empty list of top-level package names (keys in
 *                       `database/<pkg>/latest.yaml`) to include in the config.
 *                       Each name is matched case-sensitively against the
 *                       package database.
 * @param interactive    If true, prompt for grouped build options and derive
 *                       their optional packages from enabled states, then
 *                       prompt for abstract-package implementations. If false,
 *                       selections use the architecture heuristics.
 *
 * @return A YAML::Node containing the full configuration tree. The function
 *         terminates with ERROR() rather than returning on failure.
 *
 * @warning This function terminates the process via ERROR() if any package
 *          name cannot be found in the database, if the dependency graph
 *          contains a cycle that cannot be resolved, or if any required
 *          dependency has no satisfiable configuration (e.g., no known
 *          implementation for the target architecture).
 *
 * @see gen_user_config(const std::vector<std::string>&, bool, const std::string&)
 *      Overload that additionally accepts an explicit default compiler hint.
 */
YAML::Node gen_user_config(const std::vector<std::string>& package_names, bool interactive);

/**
 * @brief Generate a user-editable YAML configuration with an explicit default
 *        compiler hint.
 *
 * This overload behaves identically to the two-parameter version except that it
 * accepts an additional @p default_compiler argument which specifies the
 * compiler family (e.g. `"gcc"`, `"llvm"`, `"oneapi"`) to use as the default
 * toolchain for the generated configuration.
 *
 * When @p default_compiler is non-empty, the generator will:
 *   1. Prefer the given compiler for every package that supports it.
 *   2. Fall back to the heuristics-defined default compiler only for packages
 *      that do not list the specified compiler in their recipe.
 *
 * This is particularly useful when generating a config inside an environment
 * that has already loaded a specific compiler module (via `kez compiler load`),
 * as it ensures consistency between the loaded module and the generated plan.
 *
 * @param package_names    Non-empty list of top-level package names to include
 *                         in the config.
 * @param interactive      If true, allow interactive prompts for ambiguous
 *                         build choices.
 * @param default_compiler Compiler specification to use as the default
 *                         toolchain (e.g. `"gcc@13.4.0"`, `"llvm"`, or
 *                         `"system"`).  The `@` separator separates the
 *                         compiler name from its version; when the version
 *                         is omitted, `"latest"` is used.  Pass `"system"`
 *                         to use the system compiler.
 *
 * @return A YAML::Node containing the full abstract configuration tree. The
 *         node structure is identical to the two-parameter overload.
 *
 * @warning Same fatal-error behaviour as the two-parameter overload: missing
 *          packages, cyclic dependencies, or unsatisfiable configurations all
 *          terminate the process via ERROR().
 *
 * @see gen_user_config(const std::vector<std::string>&, bool)
 *      Simpler overload that lets the heuristics select the default compiler.
 * @see kez compiler load  Shell command that loads the compiler before config
 *                         generation.
 */
YAML::Node gen_user_config(const std::vector<std::string>& package_names, bool interactive,
                           const std::string& default_compiler);
