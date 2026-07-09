#include <sys/wait.h>

#include <algorithm>
#include <cstdlib>
#include <database/config.hpp>
#include <database/database.hpp>
#include <filesystem>
#include <iostream>
#include <parser/user_config_parser.hpp>
#include <string>
#include <ui/ui_utils.hpp>
#include <utils/bash_utils.hpp>
#include <utils/colored_io.hpp>
#include <utils/yaml_utils.hpp>

namespace {

    /**
     * @brief Resolves an abstract package name to a concrete target package.
     *
     * Looks up @p target in the user configuration's `recipe.abstract_packages` map.
     * If a mapping exists, the concrete package name is returned; otherwise the
     * original target is used unchanged.
     *
     * @param user_config  The parsed user configuration YAML node.
     * @param target       The (possibly abstract) package name to resolve.
     * @return The concrete target package name if found in `abstract_packages`,
     *         or @p target unchanged.
     */
    std::string effective_target(const YAML::Node& user_config, const std::string& target) {
        const YAML::Node recipe = user_config["recipe"];
        if (yaml_has(recipe, "abstract_packages") && recipe["abstract_packages"].IsMap() &&
            recipe["abstract_packages"][target].IsDefined()) {
            return yaml_scalar(recipe["abstract_packages"][target],
                               "abstract package selection for " + target);
        }
        return target;
    }

    /**
     * @brief Extracts the version string for a package from the user configuration.
     *
     * Reads `kez.<package>.version` from the user config. If the version contains
     * a '@' character (indicating a local source path suffix), the suffix is
     * stripped so that only the actual version identifier remains. The result is
     * validated as a safe filesystem path component.
     *
     * @param user_config  The parsed user configuration YAML node.
     * @param package      The name of the package whose version to extract.
     * @return The version string, with any local-source marker (everything after
     *         the first '@') removed.
     * @warning Terminates the program via ERROR() if `kez.<package>.version` is
     *          missing or if the version fails path-component validation.
     */
    std::string package_version(const YAML::Node& user_config, const std::string& package) {
        const YAML::Node kez = user_config["kez"];
        if (!kez.IsMap() || !kez[package].IsMap() || !yaml_has(kez[package], "version")) {
            ERROR("Invalid user configuration: package '" + package + "' has no version");
            exit(EXIT_FAILURE);
        }
        std::string version            = yaml_scalar(kez[package]["version"], package + ".version");
        const std::size_t local_source = version.find('@');
        if (local_source != std::string::npos) {
            version.erase(local_source);
        }
        validate_path_component(version, "package version");
        return version;
    }

    /**
     * @brief Extracts the compiler specification for a package from the user
     *        configuration.
     *
     * Reads `kez.<package>.compiler` from the user config. If no compiler is
     * specified, defaults to "system". The '@' character (used internally as a
     * delimiter in version strings) is replaced with '-' to produce a safe path
     * component. The result is validated as a safe filesystem path component.
     *
     * @param user_config  The parsed user configuration YAML node.
     * @param package      The name of the package whose compiler to extract.
     * @return The compiler string (with '@' replaced by '-'), or "system" if
     *         no compiler is configured.
     * @warning Terminates the program via ERROR() if the compiler value fails
     *          path-component validation.
     */
    std::string package_compiler(const YAML::Node& user_config, const std::string& package) {
        const YAML::Node kez = user_config["kez"];
        if (!kez.IsMap() || !kez[package].IsMap() || !yaml_has(kez[package], "compiler")) {
            return "system";
        }
        std::string compiler = yaml_scalar(kez[package]["compiler"], package + ".compiler");
        std::replace(compiler.begin(), compiler.end(), '@', '-');
        validate_path_component(compiler, "compiler specification");
        return compiler;
    }

}  // namespace

/**
 * @brief Converts a PackageType enum value to its human-readable string
 *        representation.
 *
 * @param type The PackageType enum value.
 * @return A string such as "package", "system", "compiler", "mpi", "vendor",
 *         "abstract", or "external". Returns "unknown" for unrecognised values.
 */
std::string package_type_name(PackageType type) {
    switch (type) {
        case PackageType::Package: return "package";
        case PackageType::System: return "system";
        case PackageType::Compiler: return "compiler";
        case PackageType::Mpi: return "mpi";
        case PackageType::Vendor: return "vendor";
        case PackageType::Abstract: return "abstract";
        case PackageType::External: return "external";
    }
    return "unknown";
}

/**
 * @brief Resolves a named path from the project's manifest.yaml.
 *
 * Reads the value of `manifest.paths.<name>` from the manifest file located
 * at `$KEZ_HOME/manifest.yaml`. If the configured path is relative, it is
 * joined with `$KEZ_WORKDIR` to form an absolute path. If it is already
 * absolute, it is returned as-is.
 *
 * @param name The key under `manifest.paths` to look up
 *             (e.g. "applications", "compilers", "system", "utilities").
 * @return The resolved (absolute) filesystem path.
 * @warning Terminates the program via ERROR() if `$KEZ_HOME` or
 *          `$KEZ_WORKDIR` is not set, or if `manifest.paths.<name>` is
 *          missing from the manifest.
 */
std::filesystem::path configured_work_path(const std::string& name) {
    const std::filesystem::path home = get_env_var("KEZ_HOME");
    const std::filesystem::path work = get_env_var("KEZ_WORKDIR");
    const YAML::Node manifest        = cached_yaml_load(home / "manifest.yaml");
    if (!yaml_has(manifest, "paths") || !manifest["paths"].IsMap() ||
        !yaml_has(manifest["paths"], name)) {
        ERROR("Invalid manifest: paths." + name + " is missing");
        exit(EXIT_FAILURE);
    }
    const std::filesystem::path configured = yaml_scalar(manifest["paths"][name], "paths." + name);
    return configured.is_absolute() ? configured : work / configured;
}

/**
 * @brief Extracts the list of target packages from the user configuration.
 *
 * Reads the `recipe.targets` sequence from the user config. Each element is
 * converted to a string and collected into a vector.
 *
 * @param user_config The parsed user configuration YAML node.
 * @return A vector of target package name strings, in the order they appear
 *         in the configuration.
 * @warning Terminates the program via ERROR() if `recipe.targets` is missing,
 *          is not a YAML sequence, or is empty.
 */
std::vector<std::string> user_config_targets(const YAML::Node& user_config) {
    if (!yaml_has(user_config, "recipe") || !user_config["recipe"].IsMap() ||
        !yaml_has(user_config["recipe"], "targets") ||
        !user_config["recipe"]["targets"].IsSequence()) {
        ERROR("Invalid user configuration: recipe.targets must be a sequence");
        exit(EXIT_FAILURE);
    }

    std::vector<std::string> result;
    for (const YAML::Node& target : user_config["recipe"]["targets"]) {
        result.push_back(yaml_scalar(target, "target package"));
    }
    if (result.empty()) {
        ERROR("Invalid user configuration: recipe.targets must not be empty");
        exit(EXIT_FAILURE);
    }
    return result;
}

/**
 * @brief Determines the installation directory prefix for a set of targets.
 *
 * Resolves all requested targets through abstract-package mapping, verifies
 * they share a common PackageType, then selects the installation prefix based
 * on that type:
 *
 *   - **Utilities** (`utilities=true`): uses the configured `utilities` path;
 *     only allowed for `PackageType::Package`.
 *   - **System**: the configured `system` path.
 *   - **Compiler**: `compilers/<package>-<version>`.
 *   - **MPI**:      `mpis/<package>-<version>-<compiler>`.
 *   - **Vendor**:   `vendors/<package>-<version>`.
 *   - **External**: rejected (configured externally, cannot be installed).
 *   - **Abstract**: rejected (must be resolved to a concrete type first).
 *   - **Package**:  `applications/<environment>`.
 *
 * For `PackageType::Package`, the environment name is obtained from the
 * @p environment_name parameter. If that is empty, `$KEZ_ACTIVE_ENV` is used
 * instead. An environment is always required for regular packages.
 *
 * @param user_config      The parsed user configuration YAML node.
 * @param environment_name The explicit environment name, or an empty string to
 *                         fall back to `$KEZ_ACTIVE_ENV`.
 * @param utilities        If true, install as a utility (forces the utilities
 *                         path and requires a single `Package` target).
 * @return The absolute installation prefix path.
 * @warning Terminates the program via ERROR() in many scenarios:
 *          - Targets have mixed package types.
 *          - `utilities=true` but the target is not a regular package.
 *          - Multiple targets of type Compiler, MPI, Vendor, or External.
 *          - Target is External (these are configured externally).
 *          - Target is unresolved Abstract.
 *          - No environment is available for a Package-type install.
 *          - Environment name fails path-component validation.
 */
std::filesystem::path installation_prefix(const YAML::Node& user_config,
                                          const std::string& environment_name, bool utilities) {
    const std::vector<std::string> targets = user_config_targets(user_config);
    PackageType common_type                = PackageType::Abstract;
    std::string selected_package;
    for (const std::string& requested : targets) {
        const std::string package = effective_target(user_config, requested);
        const PackageType type    = get_db_config(package)->type;
        if (selected_package.empty()) {
            selected_package = package;
            common_type      = type;
        } else if (common_type != type) {
            ERROR("All installation targets must have the same package type; '" + package +
                  "' is " + package_type_name(type) + " while previous targets are " +
                  package_type_name(common_type));
            exit(EXIT_FAILURE);
        }
    }

    if (utilities) {
        if (common_type != PackageType::Package) {
            ERROR("Only regular packages can be installed as utilities");
            exit(EXIT_FAILURE);
        }
        return configured_work_path("utilities");
    }

    if ((common_type == PackageType::Compiler || common_type == PackageType::Mpi ||
         common_type == PackageType::Vendor || common_type == PackageType::External) &&
        targets.size() != 1) {
        ERROR("Only one compiler, MPI, vendor, or external target can be installed at a time");
        exit(EXIT_FAILURE);
    }

    if (common_type == PackageType::System) {
        return configured_work_path("system");
    }
    if (common_type == PackageType::Compiler) {
        return configured_work_path("compilers") /
               (selected_package + "-" + package_version(user_config, selected_package));
    }
    if (common_type == PackageType::Mpi) {
        return configured_work_path("mpis") /
               (selected_package + "-" + package_version(user_config, selected_package) + "-" +
                package_compiler(user_config, selected_package));
    }
    if (common_type == PackageType::Vendor) {
        return configured_work_path("vendors") /
               (selected_package + "-" + package_version(user_config, selected_package));
    }
    if (common_type == PackageType::External) {
        ERROR("External packages are configured in config.yaml and cannot be installed");
        exit(EXIT_FAILURE);
    }
    if (common_type != PackageType::Package) {
        ERROR("Cannot install unresolved abstract package '" + selected_package + "'");
        exit(EXIT_FAILURE);
    }

    std::string selected_environment = environment_name;
    if (selected_environment.empty()) {
        selected_environment = get_env_var_noerr("KEZ_ACTIVE_ENV");
    }
    if (selected_environment.empty()) {
        ERROR("A target environment is required; pass --env <name> or run 'kez env enter <name>'");
        exit(EXIT_FAILURE);
    }
    validate_path_component(selected_environment, "environment name");
    return configured_work_path("applications") / selected_environment;
}

/**
 * @brief Validates that a string is a safe, single filesystem path component.
 *
 * A valid path component must not be empty, must not be "." or "..", and must
 * not contain any directory separator (i.e. it must not have a parent path).
 * This is used to guard against path-traversal and injection when constructing
 * installation directory names from user-supplied values such as package
 * versions, compiler specs, and environment names.
 *
 * @param value       The string to validate.
 * @param description A human-readable label describing what is being validated
 *                    (e.g. "package version", "environment name"); used in the
 *                    error message on failure.
 * @warning Terminates the program via ERROR() if the value is empty, is ".",
 *          is "..", or contains path separators.
 */
void validate_path_component(const std::string& value, const std::string& description) {
    const std::filesystem::path path(value);
    if (value.empty() || value == "." || value == ".." || path.has_parent_path() ||
        path.filename().string() != value) {
        ERROR("Invalid " + description + ": '" + value + "'");
        exit(EXIT_FAILURE);
    }
}

/**
 * @brief Executes a shell command and checks that it completed successfully.
 *
 * Delegates to std::system(3). The command is interpreted by `/bin/sh -c`.
 * After execution the exit status is inspected: the command must have exited
 * normally (not by a signal) with exit code 0.
 *
 * @param command The shell command string to execute.
 * @warning Terminates the program via ERROR() if the command could not be
 *          launched (std::system returns -1), if it did not exit normally, or
 *          if it returned a non-zero exit status.
 */
void run_external_command(const std::string& command) {
    const int status = std::system(command.c_str());
    if (status == -1 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        ERROR("Command failed: " + command);
        exit(EXIT_FAILURE);
    }
}

/**
 * @brief Lists all immediate subdirectories under a root path, sorted
 *        alphabetically.
 *
 * If @p root does not exist or contains no subdirectories, an informational
 * message is printed instead of an empty listing. Each subdirectory name is
 * printed indented by "  - ".
 *
 * @param root    The directory to scan for subdirectories.
 * @param heading A label describing the kind of entries being listed
 *                (e.g. "environments", "packages"); used in the heading and
 *                the "not found" message.
 */
void list_directories(const std::filesystem::path& root, const std::string& heading) {
    if (!std::filesystem::is_directory(root)) {
        INFO("No " + heading + " found.");
        return;
    }
    std::vector<std::string> entries;
    for (const auto& entry : std::filesystem::directory_iterator(root)) {
        if (entry.is_directory()) {
            entries.push_back(entry.path().filename().string());
        }
    }
    std::sort(entries.begin(), entries.end());
    if (entries.empty()) {
        INFO("No " + heading + " found.");
        return;
    }
    INFO("Available " + heading + ":");
    for (const std::string& entry : entries) {
        std::cout << "  - " << entry << '\n';
    }
}

/**
 * @brief Emits shell commands to activate an environment by prepending its
 *        `bin/` directory to `PATH` and setting an environment variable.
 *
 * The emitted lines are intended to be eval'd by the calling shell (typically
 * via main.sh). They export `PATH` with the prefix's `bin/` directory
 * prepended and set the given @p variable to @p value.
 *
 * @param prefix   The installation prefix whose `bin/` subdirectory is added
 *                 to `PATH`.
 * @param variable The name of the environment variable to set.
 * @param value    The value to assign to the environment variable.
 */
void emit_environment_activation(const std::filesystem::path& prefix, const std::string& variable,
                                 const std::string& value) {
    std::cout << "export PATH=" << shell_single_quote((prefix / "bin").string())
              << ":\"${PATH}\"; export " << variable << '=' << shell_single_quote(value) << '\n';
}

/**
 * @brief Emits shell commands to deactivate an environment by removing its
 *        `bin/` directory from `PATH` and unsetting an environment variable.
 *
 * The emitted lines are intended to be eval'd by the calling shell (typically
 * via main.sh). The `PATH` variable is rebuilt by iterating over its colon-
 * separated entries and excluding the entry that matches the prefix's `bin/`
 * directory. The shell-scratch variables are unset afterwards.
 *
 * @param prefix   The installation prefix whose `bin/` subdirectory is removed
 *                 from `PATH`.
 * @param variable The name of the environment variable to unset.
 */
void emit_environment_deactivation(const std::filesystem::path& prefix,
                                   const std::string& variable) {
    std::cout << "kez_remove_path=" << shell_single_quote((prefix / "bin").string())
              << "; kez_new_path=''; kez_old_ifs=\"$IFS\"; "
                 "IFS=: read -r -a kez_path_parts <<< \"$PATH\"; IFS=\"$kez_old_ifs\"; "
                 "for kez_path_entry in \"${kez_path_parts[@]}\"; do "
                 "if [ \"$kez_path_entry\" != \"$kez_remove_path\" ]; then "
                 "if [ -z \"$kez_new_path\" ]; then kez_new_path=\"$kez_path_entry\"; "
                 "else kez_new_path=\"$kez_new_path:$kez_path_entry\"; fi; fi; done; "
                 "export PATH=\"$kez_new_path\"; unset "
              << variable
              << "; unset kez_remove_path kez_new_path kez_old_ifs kez_path_entry kez_path_parts\n";
}

/**
 * @brief Prints a human-readable representation of a BashCommandPlan to
 *        stdout.
 *
 * For each PackageCommands entry in the plan, a heading line showing the
 * package name is printed, followed by each command prefixed with " -  ".
 *
 * @param plan The BashCommandPlan to display (a sequence of PackageCommands
 *             entries, each containing a package name and its list of shell
 *             commands).
 */
void print_command_plan(const BashCommandPlan& plan) {
    for (const PackageCommands& package : plan) {
        INFO("Instructions for " + package.package + ":");
        for (const std::string& command : package.commands) {
            std::cout << " -  " << command << '\n';
        }
    }
}
