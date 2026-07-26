#include <cstdlib>
#include <string>
#include <ui/argparse.hpp>
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
        INFO("Kez - an HPC-focused package manager\n\n"
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
             "  dbcheck     Parse and validate the package database\n\n"
             "Run 'kez <command> --help' for command-specific help.");
    }
}  // namespace

void run_ui(int argc, char* argv[]) {
    CommandArguments arguments;
    arguments.reserve(static_cast<std::size_t>(argc - 1));
    for (int index = 1; index < argc; ++index) {
        arguments.emplace_back(argv[index]);
    }

    const UiArgumentsParseResult parsed = parse_ui_arguments(arguments);
    if (!parsed.error.empty()) {
        ERROR(parsed.error);
        print_help();
        exit(EXIT_FAILURE);
    }

    switch (parsed.command) {
        case UiCommand::Help: print_help(); return;
        case UiCommand::Version: {
            const std::filesystem::path kez_home = get_env_var("KEZ_HOME");
            const YAML::Node manifest            = cached_yaml_load(kez_home / "manifest.yaml");
            const std::string version            = manifest["project"]["version"].as<std::string>();
            INFO("Kez version: " + version);
            return;
        }
        case UiCommand::Init: execute_init(parsed.arguments); return;
        case UiCommand::Update: execute_update(parsed.arguments); return;
        case UiCommand::Install: execute_install(parsed.arguments); return;
        case UiCommand::Utilities: execute_utilities(parsed.arguments); return;
        case UiCommand::Uconf: execute_uconf(parsed.arguments); return;
        case UiCommand::Environment: execute_environment(parsed.arguments); return;
        case UiCommand::Compiler: execute_compiler(parsed.arguments); return;
        case UiCommand::MPI: execute_mpi(parsed.arguments); return;
        case UiCommand::Factory: execute_factory(parsed.arguments); return;
        case UiCommand::Info: execute_info(parsed.arguments); return;
        case UiCommand::DbCheck: execute_dbcheck(parsed.arguments); return;
    }
}
