#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <ui/argparse.hpp>
#include <ui/commands.hpp>
#include <ui/ui_utils.hpp>
#include <utils/bash_utils.hpp>
#include <utils/colored_io.hpp>
#include <utils/file_utils.hpp>

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
        if (fs_exists(path)) {
            ERROR(description + " already exists: " + path.filename().string());
            exit(EXIT_FAILURE);
        }
        fs_create_dirs(path);
        SUCCESS(description + " created: " + path.filename().string());
    }

    /** @brief Remove a managed directory, erroring if it does not exist. */
    void remove_directory(const std::filesystem::path& path, const std::string& description) {
        if (!fs_directory(path)) {
            ERROR(description + " does not exist: " + path.filename().string());
            exit(EXIT_FAILURE);
        }
        fs_remove_all(path);
        SUCCESS(description + " removed: " + path.filename().string());
    }

    /** @brief Remove the modulefile associated with an environment path, if it exists. */
    void remove_modulefile(const std::filesystem::path& env_path) {
        const std::filesystem::path modulefiles_dir = configured_work_path("modulefiles");
        const std::filesystem::path modulefile      = modulefiles_dir / env_path.filename();
        if (fs_exists(modulefile)) {
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
        if (!fs_directory(path)) {
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
        const ManagedEnvironmentArgumentsParseResult parsed =
            parse_managed_environment_arguments(arguments, command);
        if (!parsed.error.empty()) {
            ERROR(parsed.error);
            exit(EXIT_FAILURE);
        }
        if (parsed.arguments.action == ManagedEnvironmentAction::Help) {
            managed_help(command);
            return;
        }

        const std::filesystem::path root = configured_work_path(manifest_path);
        switch (parsed.arguments.action) {
            case ManagedEnvironmentAction::Help: return;
            case ManagedEnvironmentAction::List: list_directories(root, plural); return;
            case ManagedEnvironmentAction::Which: {
                const std::string current = get_env_var_noerr(variable);
                INFO(current.empty() ? "No " + singular + " is currently loaded."
                                     : "Currently loaded " + singular + ": " + current);
                return;
            }
            case ManagedEnvironmentAction::Load: {
                const std::string& name = parsed.arguments.name;
                validate_path_component(name, command + " load name");
                const std::string current = get_env_var_noerr(variable);
                if (!current.empty()) {
                    ERROR("A " + singular + " is already loaded: " + current);
                    exit(EXIT_FAILURE);
                }
                const std::filesystem::path prefix = root / name;
                if (!fs_directory(prefix)) {
                    ERROR(singular + " does not exist: " + name);
                    exit(EXIT_FAILURE);
                }
                emit_environment_activation(prefix, variable, name);
                return;
            }
            case ManagedEnvironmentAction::Unload: {
                const std::string name =
                    get_env_var(variable, "No " + singular + " is currently loaded");
                validate_path_component(name, singular + " name");
                emit_environment_deactivation(root / name, variable);
                return;
            }
            case ManagedEnvironmentAction::Remove: {
                const std::string& name = parsed.arguments.name;
                validate_path_component(name, command + " remove name");
                const std::filesystem::path path = root / name;
                remove_directory(path, singular);
                remove_modulefile(path);
                return;
            }
        }
    }
}  // namespace

/** @brief Dispatch environment subcommands: list, which, deactivate, create, remove, empty, activate. */
void execute_environment(const CommandArguments& arguments) {
    const EnvironmentArgumentsParseResult parsed = parse_environment_arguments(arguments);
    if (!parsed.error.empty()) {
        ERROR(parsed.error);
        exit(EXIT_FAILURE);
    }
    if (parsed.arguments.action == EnvironmentAction::Help) {
        environment_help();
        return;
    }

    const std::filesystem::path root = configured_work_path("applications");
    const auto environment_path      = [&parsed, &root]() {
        validate_path_component(parsed.arguments.name, "environment name");
        return root / parsed.arguments.name;
    };
    switch (parsed.arguments.action) {
        case EnvironmentAction::Help: return;
        case EnvironmentAction::List: list_directories(root, "environments"); return;
        case EnvironmentAction::Which: {
            const std::string current = get_env_var_noerr("KEZ_ACTIVE_ENV");
            INFO(current.empty() ? "No application environment is currently active."
                                 : "Current application environment: " + current);
            return;
        }
        case EnvironmentAction::Deactivate: {
            const std::string name =
                get_env_var("KEZ_ACTIVE_ENV", "No application environment is currently active");
            validate_path_component(name, "environment name");
            emit_environment_deactivation(root / name, "KEZ_ACTIVE_ENV");
            return;
        }
        case EnvironmentAction::Create:
            create_managed_directory(environment_path(), "Environment");
            return;
        case EnvironmentAction::Remove: {
            const std::filesystem::path path = environment_path();
            remove_directory(path, "Environment");
            remove_modulefile(path);
            return;
        }
        case EnvironmentAction::Empty: empty_directory(environment_path(), "Environment"); return;
        case EnvironmentAction::Activate: {
            const std::filesystem::path path = environment_path();
            const std::string current        = get_env_var_noerr("KEZ_ACTIVE_ENV");
            if (!current.empty()) {
                ERROR("An application environment is already active: " + current);
                exit(EXIT_FAILURE);
            }
            if (!fs_directory(path)) {
                ERROR("Environment does not exist: " + parsed.arguments.name);
                exit(EXIT_FAILURE);
            }
            emit_environment_activation(path, "KEZ_ACTIVE_ENV", parsed.arguments.name);
            return;
        }
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
