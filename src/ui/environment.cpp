#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <ui/commands.hpp>
#include <ui/ui_utils.hpp>
#include <utils/bash_utils.hpp>
#include <utils/colored_io.hpp>

namespace {
    /**
     * @brief Print the usage message for the `env` top-level subcommand.
     *
     * Displays a single-line synopsis of the supported `kez env` actions
     * (create, remove, list, enter, exit, which, empty) to stdout.
     *
     * This is a terminal helper; it never terminates the process.
     */
    void environment_help() {
        std::cout << "Usage: kez env <create|remove|list|enter|exit|which|empty> [name]\n";
    }

    /**
     * @brief Print the usage message for a managed-environment subcommand
     *        (compiler or mpi).
     *
     * Displays a single-line synopsis of the supported actions (load, unload,
     * list, which, remove) for the given subcommand name.
     *
     * @param command  The subcommand name (e.g. "compiler" or "mpi") that
     *                 appears in the produced usage line.
     *
     * This is a terminal helper; it never terminates the process.
     */
    void managed_help(const std::string& command) {
        std::cout << "Usage: kez " << command << " <load|unload|list|which|remove> [name]\n";
    }

    /**
     * @brief Extract and validate a required name argument from the command
     *        line.
     *
     * Asserts that exactly two positional tokens are present (the action and
     * the name). If the count is wrong, the program terminates with an error.
     * Otherwise the second token is validated as a safe path component via
     * @c validate_path_component and returned.
     *
     * @param arguments  The full command-line argument vector for the
     *                   subcommand.  @c arguments[0] is the action,
     *                   @c arguments[1] is the name.
     * @param action     A human-readable label describing the enclosing action
     *                   (e.g. "compiler load", "env create"), used in the
     *                   error diagnostic when the argument count is wrong.
     *
     * @return The validated name string from @c arguments[1].
     *
     * @warning Terminates the process with @c exit(EXIT_FAILURE) when
     *          @c arguments.size() != 2, or when
     *          @c validate_path_component rejects the name.
     */
    std::string required_name(const CommandArguments& arguments, const std::string& action) {
        if (arguments.size() != 2) {
            ERROR(action + " requires exactly one name");
            exit(EXIT_FAILURE);
        }
        validate_path_component(arguments[1], action + " name");
        return arguments[1];
    }

    /**
     * @brief Create a managed directory (environment, compiler, or MPI prefix).
     *
     * If the directory already exists the operation is treated as a fatal
     * error.  Otherwise the directory (including any missing parents) is
     * created via @c std::filesystem::create_directories.  On success a
     * SUCCESS message is printed.
     *
     * @param path         The absolute or relative filesystem path to create.
     * @param description  A human-readable label for the entity being created
     *                     (e.g. "Environment", "Compiler"), used in both
     *                     error and success diagnostics.
     *
     * @warning Terminates the process with @c exit(EXIT_FAILURE) if the path
     *          already exists or if filesystem creation fails.
     */
    void create_managed_directory(const std::filesystem::path& path,
                                  const std::string& description) {
        if (std::filesystem::exists(path)) {
            ERROR(description + " already exists: " + path.filename().string());
            exit(EXIT_FAILURE);
        }
        std::error_code error;
        std::filesystem::create_directories(path, error);
        if (error) {
            ERROR("Failed to create " + description + ": " + error.message());
            exit(EXIT_FAILURE);
        }
        SUCCESS(description + " created: " + path.filename().string());
    }

    /**
     * @brief Remove a managed directory and all of its contents.
     *
     * If the path does not exist or is not a directory the operation is
     * treated as a fatal error.  The entire directory tree is removed
     * recursively via @c std::filesystem::remove_all.  On success a SUCCESS
     * message is printed.
     *
     * @param path         The filesystem path of the directory to remove.
     * @param description  A human-readable label for the entity being removed
     *                     (e.g. "Environment", "Compiler"), used in error
     *                     and success diagnostics.
     *
     * @warning Terminates the process with @c exit(EXIT_FAILURE) if the path
     *          is not a directory or if the recursive removal fails.
     */
    void remove_directory(const std::filesystem::path& path, const std::string& description) {
        if (!std::filesystem::is_directory(path)) {
            ERROR(description + " does not exist: " + path.filename().string());
            exit(EXIT_FAILURE);
        }
        std::error_code error;
        std::filesystem::remove_all(path, error);
        if (error) {
            ERROR("Failed to remove " + description + ": " + error.message());
            exit(EXIT_FAILURE);
        }
        SUCCESS(description + " removed: " + path.filename().string());
    }

    /**
     * @brief Remove the Tcl modulefile associated with an environment.
     *
     * Looks up the modulefiles directory under the configured work path and
     * deletes the modulefile whose filename matches the environment directory's
     * basename.  If the modulefile does not exist the function does nothing.
     * If the deletion fails a non-fatal WARNING is printed; on success an
     * INFO message is printed.
     *
     * @param env_path  Path of the environment directory.  Only the filename
     *                  (basename) is used to identify the corresponding
     *                  modulefile.
     *
     * @note Unlike the other filesystem helpers in this file, this function
     *       never terminates the process.  A failure to remove the modulefile
     *       is logged as a warning but does not block the containing operation.
     */
    void remove_modulefile(const std::filesystem::path& env_path) {
        const std::filesystem::path modulefiles_dir = configured_work_path("modulefiles");
        const std::filesystem::path modulefile      = modulefiles_dir / env_path.filename();
        if (std::filesystem::exists(modulefile)) {
            std::error_code error;
            std::filesystem::remove(modulefile, error);
            if (error) {
                WARNING("Could not remove module file: " + error.message());
            } else {
                INFO("Module file removed: " + modulefile.filename().string());
            }
        }
    }

    /**
     * @brief Remove all contents of a managed directory while keeping the
     *        directory itself.
     *
     * Iterates over every entry in the given directory and removes each one
     * recursively.  If the directory does not exist or is not a directory the
     * operation is treated as a fatal error.  On success a SUCCESS message is
     * printed.
     *
     * @param path         The filesystem path of the directory to empty.
     * @param description  A human-readable label for the entity being emptied
     *                     (e.g. "Environment"), used in error and success
     *                     diagnostics.
     *
     * @warning Terminates the process with @c exit(EXIT_FAILURE) if the path
     *          is not a directory or if any individual removal fails.
     */
    void empty_directory(const std::filesystem::path& path, const std::string& description) {
        if (!std::filesystem::is_directory(path)) {
            ERROR(description + " does not exist: " + path.filename().string());
            exit(EXIT_FAILURE);
        }
        std::error_code error;
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            std::filesystem::remove_all(entry.path(), error);
            if (error) {
                ERROR("Failed to empty " + description + ": " + error.message());
                exit(EXIT_FAILURE);
            }
        }
        SUCCESS(description + " emptied: " + path.filename().string());
    }

    /**
     * @brief Dispatch sub-actions for a managed environment subcommand
     *        (compiler or mpi).
     *
     * This is the shared handler that powers both `kez compiler <action>`
     * and `kez mpi <action>`.  It interprets the first positional token as
     * the action and dispatches to the appropriate logic:
     *
     *   - **load**:   Requires a name; checks that no compiler/MPI is already
     *                 loaded, then emits shell-level activation commands.
     *   - **unload**: Reads the currently loaded value from the environment
     *                 variable; emits deactivation commands.
     *   - **list**:   Lists all subdirectories under the managed root.
     *   - **which**:  Prints the currently loaded value (or "none").
     *   - **remove**: Requires a name; removes the directory and its
     *                 associated modulefile.
     *
     * If the first token is `-h` or `--help`, the managed help text is
     * printed instead and the function returns early.
     *
     * @param arguments      The command-line tokens following the subcommand
     *                       name (e.g. after "compiler").
     * @param command        The subcommand name string used in help and error
     *                       messages (e.g. "compiler", "mpi").
     * @param manifest_path  The relative subdirectory under KEZ_WORKDIR that
     *                       holds the managed entities
     *                       (e.g. "compilers", "mpis").
     * @param variable       The environment variable name used to track the
     *                       currently loaded entity
     *                       (e.g. "KEZ_COMPILER", "KEZ_MPI").
     * @param singular       A singular display label for the managed entity
     *                       (e.g. "compiler", "MPI environment").
     * @param plural         A plural display label for the managed entity
     *                       (e.g. "compilers", "MPI environments").
     *
     * @warning Terminates the process with @c exit(EXIT_FAILURE) for:
     *          - An unrecognized action.
     *          - `list` or `which` with extra arguments.
     *          - `load` with a missing or already-set environment.
     *          - `unload` when no environment is currently loaded.
     *          - `remove` with a missing name.
     */
    void execute_managed_environment(const CommandArguments& arguments, const std::string& command,
                                     const std::string& manifest_path, const std::string& variable,
                                     const std::string& singular, const std::string& plural) {
        if (arguments.empty() || arguments.front() == "-h" || arguments.front() == "--help") {
            managed_help(command);
            return;
        }

        const std::string& action        = arguments.front();
        const std::filesystem::path root = configured_work_path(manifest_path);
        if (action == "list") {
            if (arguments.size() != 1) {
                ERROR(command + " list does not accept additional arguments");
                exit(EXIT_FAILURE);
            }
            list_directories(root, plural);
            return;
        }
        if (action == "which") {
            if (arguments.size() != 1) {
                ERROR(command + " which does not accept additional arguments");
                exit(EXIT_FAILURE);
            }
            const std::string current = get_env_var_noerr(variable);
            INFO(current.empty() ? "No " + singular + " is currently loaded."
                                 : "Currently loaded " + singular + ": " + current);
            return;
        }
        if (action == "load") {
            const std::string name    = required_name(arguments, command + " load");
            const std::string current = get_env_var_noerr(variable);
            if (!current.empty()) {
                ERROR("A " + singular + " is already loaded: " + current);
                exit(EXIT_FAILURE);
            }
            const std::filesystem::path prefix = root / name;
            if (!std::filesystem::is_directory(prefix)) {
                ERROR(singular + " does not exist: " + name);
                exit(EXIT_FAILURE);
            }
            emit_environment_activation(prefix, variable, name);
            return;
        }
        if (action == "unload") {
            if (arguments.size() != 1) {
                ERROR(command + " unload does not accept additional arguments");
                exit(EXIT_FAILURE);
            }
            const std::string name =
                get_env_var(variable, "No " + singular + " is currently loaded");
            validate_path_component(name, singular + " name");
            emit_environment_deactivation(root / name, variable);
            return;
        }
        if (action == "remove") {
            const std::string name           = required_name(arguments, command + " remove");
            const std::filesystem::path path = root / name;
            remove_directory(path, singular);
            remove_modulefile(path);
            return;
        }

        ERROR("Unknown " + command + " command: " + action);
        exit(EXIT_FAILURE);
    }
}  // namespace

/**
 * @brief Dispatch sub-actions for the `kez env` top-level subcommand.
 *
 * Interprets the first positional token as the action and dispatches to the
 * appropriate logic:
 *
 *   - **create**: Creates a new empty environment directory under
 *                 KEZ_WORKDIR/applications/.
 *   - **remove**: Removes an environment directory and its associated
 *                 modulefile.
 *   - **list**:   Lists all existing environments.
 *   - **enter**:  Activates an environment in the current shell, setting
 *                 KEZ_ACTIVE_ENV.  Refuses if an environment is already
 *                 active.
 *   - **exit**:   Deactivates the current environment by clearing
 *                 KEZ_ACTIVE_ENV.
 *   - **which**:  Prints the currently active environment (or "none").
 *   - **empty**:  Removes all packages from an environment while keeping
 *                 the directory itself.
 *
 * If the first token is `-h` or `--help`, the environment help text is
 * printed and the function returns early.
 *
 * @param arguments  The command-line tokens following the `env` subcommand.
 *                   The first token is the action; `create`, `remove`,
 *                   `enter`, and `empty` require a second token giving
 *                   the environment name.
 *
 * @warning Terminates the process with @c exit(EXIT_FAILURE) for:
 *          - An unrecognized action.
 *          - `list`, `which`, or `exit` with extra arguments.
 *          - `create` when the target directory already exists.
 *          - `enter` when an environment is already active or the target
 *            does not exist.
 *          - `remove` or `empty` when the target does not exist.
 *          - Any filesystem operation failure.
 *
 * @note Shell-affecting commands (`enter`, `exit`) emit shell-evaluated
 *       output that must be consumed by the bash wrapper in @c main.sh.
 *       The emitted commands set or clear the @c KEZ_ACTIVE_ENV environment
 *       variable.
 *
 * @see execute_compiler
 * @see execute_mpi
 */
void execute_environment(const CommandArguments& arguments) {
    if (arguments.empty() || arguments.front() == "-h" || arguments.front() == "--help") {
        environment_help();
        return;
    }

    const std::string& action        = arguments.front();
    const std::filesystem::path root = configured_work_path("applications");
    if (action == "list") {
        if (arguments.size() != 1) {
            ERROR("env list does not accept additional arguments");
            exit(EXIT_FAILURE);
        }
        list_directories(root, "environments");
        return;
    }
    if (action == "which") {
        if (arguments.size() != 1) {
            ERROR("env which does not accept additional arguments");
            exit(EXIT_FAILURE);
        }
        const std::string current = get_env_var_noerr("KEZ_ACTIVE_ENV");
        INFO(current.empty() ? "No application environment is currently active."
                             : "Current application environment: " + current);
        return;
    }
    if (action == "exit") {
        if (arguments.size() != 1) {
            ERROR("env exit does not accept additional arguments");
            exit(EXIT_FAILURE);
        }
        const std::string name =
            get_env_var("KEZ_ACTIVE_ENV", "No application environment is currently active");
        validate_path_component(name, "environment name");
        emit_environment_deactivation(root / name, "KEZ_ACTIVE_ENV");
        return;
    }

    const std::string name           = required_name(arguments, "env " + action);
    const std::filesystem::path path = root / name;
    if (action == "create") {
        create_managed_directory(path, "Environment");
    } else if (action == "remove") {
        remove_directory(path, "Environment");
        remove_modulefile(path);
    } else if (action == "empty") {
        empty_directory(path, "Environment");
    } else if (action == "enter") {
        const std::string current = get_env_var_noerr("KEZ_ACTIVE_ENV");
        if (!current.empty()) {
            ERROR("An application environment is already active: " + current);
            exit(EXIT_FAILURE);
        }
        if (!std::filesystem::is_directory(path)) {
            ERROR("Environment does not exist: " + name);
            exit(EXIT_FAILURE);
        }
        emit_environment_activation(path, "KEZ_ACTIVE_ENV", name);
    } else {
        ERROR("Unknown env command: " + action);
        exit(EXIT_FAILURE);
    }
}

/**
 * @brief Dispatch sub-actions for the `kez compiler` subcommand.
 *
 * Delegates entirely to @c execute_managed_environment, configuring it for
 * the compiler domain:
 *   - Root directory: KEZ_WORKDIR/compilers/
 *   - Environment variable: KEZ_COMPILER
 *   - Display labels: "compiler" / "compilers"
 *
 * @param arguments  Command-line tokens after the `compiler` subcommand.
 *                   See @c execute_managed_environment for supported actions
 *                   (load, unload, list, which, remove).
 *
 * @see execute_managed_environment
 * @see execute_mpi
 */
void execute_compiler(const CommandArguments& arguments) {
    execute_managed_environment(arguments, "compiler", "compilers", "KEZ_COMPILER", "compiler",
                                "compilers");
}

/**
 * @brief Dispatch sub-actions for the `kez mpi` subcommand.
 *
 * Delegates entirely to @c execute_managed_environment, configuring it for
 * the MPI domain:
 *   - Root directory: KEZ_WORKDIR/mpis/
 *   - Environment variable: KEZ_MPI
 *   - Display labels: "MPI environment" / "MPI environments"
 *
 * @param arguments  Command-line tokens after the `mpi` subcommand.
 *                   See @c execute_managed_environment for supported actions
 *                   (load, unload, list, which, remove).
 *
 * @see execute_managed_environment
 * @see execute_compiler
 */
void execute_mpi(const CommandArguments& arguments) {
    execute_managed_environment(arguments, "mpi", "mpis", "KEZ_MPI", "MPI environment",
                                "MPI environments");
}
