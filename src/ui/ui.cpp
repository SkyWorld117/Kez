#include <cstdlib>
#include <iostream>
#include <string>
#include <ui/commands.hpp>
#include <ui/ui.hpp>
#include <utils/bash_utils.hpp>
#include <utils/colored_io.hpp>
#include <utils/yaml_utils.hpp>

namespace {
    /**
     * @brief Prints the top-level usage help text to stdout.
     *
     * Lists every available command with a one-line summary and directs the user
     * to `kez <command> --help` for per-command details. Called when no argument
     * is given, or when `-h` / `--help` is passed.
     *
     * @note This function does not terminate the program; the caller decides
     *       whether to return or exit after calling it.
     */
    void print_help() {
        std::cout << "Kez - an HPC-focused package manager\n\n"
                     "Usage: kez <command> [options]\n\n"
                     "Commands:\n"
                     "  init        Initialize or refresh the Kez toolchain\n"
                     "  update      Update the source tree and rebuild Kez\n"
                     "  install     Install packages into an environment\n"
                     "  uconf       Generate or inspect a user configuration\n"
                     "  utilities   Manage the shared utilities environment\n"
                     "  env         Manage application environments\n"
                     "  compiler    Manage installed compiler environments\n"
                     "  mpi         Manage installed MPI environments\n"
                     "  factory     Manage batch build and profiling factories\n"
                     "  info        Show package metadata\n"
                     "  selfcheck   Parse and validate the package database\n\n"
                     "Run 'kez <command> --help' for command-specific help.\n";
    }
}  // namespace

void run_ui(int argc, char* argv[]) {
    if (argc <= 1) {
        print_help();
        return;
    }

    const std::string command = argv[1];
    if (command == "-h" || command == "--help") {
        print_help();
        return;
    }
    if (command == "-V" || command == "--version") {
        std::filesystem::path kez_home = get_env_var("KEZ_HOME");
        YAML::Node manifest            = cached_yaml_load(kez_home / "manifest.yaml");
        std::string version            = manifest["project"]["version"].as<std::string>();
        INFO("Kez version: " + version);
        return;
    }

    CommandArguments arguments;
    arguments.reserve(static_cast<std::size_t>(argc - 2));
    for (int index = 2; index < argc; ++index) {
        arguments.emplace_back(argv[index]);
    }

    if (command == "init") {
        execute_init(arguments);
    } else if (command == "update") {
        execute_update(arguments);
    } else if (command == "install") {
        execute_install(arguments);
    } else if (command == "utilities") {
        execute_utilities(arguments);
    } else if (command == "uconf") {
        execute_uconf(arguments);
    } else if (command == "env") {
        execute_environment(arguments);
    } else if (command == "compiler") {
        execute_compiler(arguments);
    } else if (command == "mpi") {
        execute_mpi(arguments);
    } else if (command == "factory") {
        execute_factory(arguments);
    } else if (command == "info") {
        execute_info(arguments);
    } else if (command == "selfcheck") {
        execute_selfcheck(arguments);
    } else {
        ERROR("Unknown command: " + command);
        print_help();
        exit(EXIT_FAILURE);
    }
}
