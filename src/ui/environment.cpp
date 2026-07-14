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
     * (create, remove, list, activate, deactivate, which, empty) to stdout.
     *
     * This is a terminal helper; it never terminates the process.
     */
    void environment_help() {
        std::cout << "Usage: kez env <create|remove|list|activate|deactivate|which|empty> [name]\n";
    }

    /** @brief Print the usage message for a managed subcommand (compiler/mpi). */
    void managed_help(const std::string& command) {
        std::cout << "Usage: kez " << command << " <load|unload|list|which|remove> [name]\n";
    }

    /** @brief Create a managed directory, erroring if it already exists. */
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

    /** @brief Remove a managed directory, erroring if it does not exist. */
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

    /** @brief Remove the modulefile associated with an environment path, if it exists. */
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

    /** @brief Remove all contents of a managed directory while keeping the directory itself. */
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

    /** @brief Dispatch load/unload/list/which/remove actions for a managed environment (compiler or mpi). */
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

/** @brief Dispatch environment subcommands: list, which, deactivate, create, remove, empty, activate. */
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
    if (action == "deactivate") {
        if (arguments.size() != 1) {
            ERROR("env deactivate does not accept additional arguments");
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
    } else if (action == "activate") {
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

/** @brief Delegate compiler subcommands to the managed-environment handler. */
void execute_compiler(const CommandArguments& arguments) {
    execute_managed_environment(arguments, "compiler", "compilers", "KEZ_COMPILER", "compiler",
                                "compilers");
}

/** @brief Delegate MPI subcommands to the managed-environment handler. */
void execute_mpi(const CommandArguments& arguments) {
    execute_managed_environment(arguments, "mpi", "mpis", "KEZ_MPI", "MPI environment",
                                "MPI environments");
}
