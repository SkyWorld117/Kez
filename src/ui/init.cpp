/**
 * @file init.cpp
 * @brief Implements the `kez init` and `kez update` subcommands.
 *
 * `execute_init` bootstraps the system toolchain by delegating to
 * `scripts/init.sh`.  `execute_update` pulls the latest source, rebuilds the
 * project, and optionally refreshes the system environment.
 */

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <ui/argparse.hpp>
#include <ui/commands.hpp>
#include <ui/ui_utils.hpp>
#include <utils/bash_utils.hpp>
#include <utils/colored_io.hpp>

/**
 * @brief Executes the `kez init` command.
 *
 * Delegates to scripts/init.sh, optionally passing --refresh and/or
 * --use-distro-compiler flags.
 */
void execute_init(const CommandArguments& arguments) {
    const InitOptionsParseResult parsed = parse_init_options(arguments);
    if (!parsed.error.empty()) {
        ERROR(parsed.error);
        exit(EXIT_FAILURE);
    }
    if (parsed.help) {
        std::cout << "Usage: kez init [--refresh] [--use-distro-compiler]\n\n"
                     "  --refresh               Recreate the system environment\n"
                     "  --use-distro-compiler   Link the distribution compiler instead of "
                     "building GCC\n";
        return;
    }

    const std::filesystem::path script =
        std::filesystem::path(get_env_var("KEZ_HOME")) / "scripts" / "init.sh";
    if (!std::filesystem::is_regular_file(script)) {
        ERROR("Initialization script does not exist: " + script.string());
        exit(EXIT_FAILURE);
    }
    std::string command = "bash " + shell_single_quote(script.string());
    if (parsed.options.refresh) {
        command += " --refresh";
    }
    if (parsed.options.use_distro_compiler) {
        command += " --use-distro-compiler";
    }
    run_external_command(command);
}

/**
 * @brief Executes the `kez update` command.
 *
 * Pulls the latest source via git, rebuilds the project, and optionally
 * refreshes the system toolchain via scripts/init.sh --refresh.
 */
void execute_update(const CommandArguments& arguments) {
    const UpdateOptionsParseResult parsed = parse_update_options(arguments);
    if (!parsed.error.empty()) {
        ERROR(parsed.error);
        exit(EXIT_FAILURE);
    }
    if (parsed.help) {
        std::cout << "Usage: kez update [--with-system]\n\n"
                     "  --with-system  Refresh the system toolchain after rebuilding Kez\n";
        return;
    }

    const std::filesystem::path home = get_env_var("KEZ_HOME");
    std::string jobs                 = get_env_var_noerr("KEZ_NPROC", "1");
    if (jobs.empty() || jobs.find_first_not_of('0') == std::string::npos ||
        !std::all_of(jobs.begin(), jobs.end(), [](const char character) {
            return std::isdigit(static_cast<unsigned char>(character));
        })) {
        ERROR("KEZ_NPROC must be a positive integer");
        exit(EXIT_FAILURE);
    }

    run_external_command("git -C " + shell_single_quote(home.string()) + " pull --ff-only");
    run_external_command("make -C " + shell_single_quote(home.string()) + " -B -j" + jobs);
    if (parsed.options.with_system) {
        run_external_command("bash " + shell_single_quote((home / "scripts" / "init.sh").string()) +
                             " --refresh");
    }
    SUCCESS("Kez updated. Reload the shell to use the rebuilt executable.");
}
