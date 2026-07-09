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
#include <ui/commands.hpp>
#include <ui/ui_utils.hpp>
#include <utils/bash_utils.hpp>
#include <utils/colored_io.hpp>

/**
 * @brief Execute the `kez init` subcommand.
 *
 * Parses command-line flags and launches the bootstrap script
 * (`$KEZ_HOME/scripts/init.sh`) to create or refresh the distro-independent
 * system stack.
 *
 * Supported options:
 *   - `--refresh`             : Recreate the system environment from scratch.
 *   - `--use-distro-compiler` : Skip building GCC and link the distribution's
 *                               compiler instead.
 *   - `-h` / `--help`         : Print usage information and return early.
 *
 * The script path is constructed from the `KEZ_HOME` environment variable and
 * guarded by an existence check.  The final command is executed synchronously
 * via @ref run_external_command.
 *
 * @param arguments  Vector of command-line tokens following the `init`
 *                   subcommand.  Each token is matched case-sensitively
 *                   against the known flags above.  Unknown tokens cause
 *                   an immediate error.
 *
 * @return void (returns early when `--help` is given; otherwise the function
 *         only returns after `init.sh` completes).
 *
 * @note Terminates the process with `exit(EXIT_FAILURE)` if:
 *       - An unknown option is encountered.
 *       - `$KEZ_HOME` is not set (caught by @ref get_env_var).
 *       - `$KEZ_HOME/scripts/init.sh` does not exist or is not a regular file.
 *       - The external command returns a non-zero exit status (caught by
 *         @ref run_external_command).
 *
 * @see execute_update
 * @see run_external_command
 * @see shell_single_quote
 */
void execute_init(const CommandArguments& arguments) {
    bool refresh             = false;
    bool use_distro_compiler = false;
    for (const std::string& argument : arguments) {
        if (argument == "-h" || argument == "--help") {
            std::cout << "Usage: kez init [--refresh] [--use-distro-compiler]\n\n"
                         "  --refresh               Recreate the system environment\n"
                         "  --use-distro-compiler   Link the distribution compiler instead of "
                         "building GCC\n";
            return;
        }
        if (argument == "--refresh") {
            refresh = true;
        } else if (argument == "--use-distro-compiler") {
            use_distro_compiler = true;
        } else {
            ERROR("Unknown init option: " + argument);
            exit(EXIT_FAILURE);
        }
    }

    const std::filesystem::path script =
        std::filesystem::path(get_env_var("KEZ_HOME")) / "scripts" / "init.sh";
    if (!std::filesystem::is_regular_file(script)) {
        ERROR("Initialization script does not exist: " + script.string());
        exit(EXIT_FAILURE);
    }
    std::string command = "bash " + shell_single_quote(script.string());
    if (refresh) {
        command += " --refresh";
    }
    if (use_distro_compiler) {
        command += " --use-distro-compiler";
    }
    run_external_command(command);
}

/**
 * @brief Execute the `kez update` subcommand.
 *
 * Updates the Kez source tree by pulling from its remote (`git pull
 * --ff-only`), then rebuilds the project with `make -B`.  When
 * `--with-system` is passed the system environment is also refreshed
 * after the rebuild.
 *
 * The number of parallel make jobs is controlled by the `KEZ_NPROC`
 * environment variable; it defaults to `"1"` when not set and must be
 * a positive integer.
 *
 * Supported options:
 *   - `--with-system` : Refresh the system toolchain after rebuilding Kez.
 *   - `-h` / `--help` : Print usage information and return early.
 *
 * @param arguments  Vector of command-line tokens following the `update`
 *                   subcommand.  Each token is matched case-sensitively
 *                   against the known flags above.  Unknown tokens cause
 *                   an immediate error.
 *
 * @return void (returns early when `--help` is given; otherwise the function
 *         only returns after the update and optional refresh complete).
 *
 * @note Terminates the process with `exit(EXIT_FAILURE)` if:
 *       - `KEZ_NPROC` is not set to a valid positive integer (or is empty or
 *         consists only of zeros).
 *       - `$KEZ_HOME` is not set (caught by @ref get_env_var).
 *       - The `git pull` or `make` command returns a non-zero exit status
 *         (caught by @ref run_external_command).
 *
 * @warning The `KEZ_NPROC` validation rejects the string if it contains any
 *          non-digit character or if every digit is zero (i.e.
 *          `find_first_not_of('0')` returns @c npos).  This also rejects the
 *          string `"0"` itself.
 *
 * @see execute_init
 * @see run_external_command
 * @see shell_single_quote
 */
void execute_update(const CommandArguments& arguments) {
    bool with_system = false;
    for (const std::string& argument : arguments) {
        if (argument == "-h" || argument == "--help") {
            std::cout << "Usage: kez update [--with-system]\n\n"
                         "  --with-system  Refresh the system toolchain after rebuilding Kez\n";
            return;
        }
        if (argument == "--with-system") {
            with_system = true;
        } else {
            ERROR("Unknown update option: " + argument);
            exit(EXIT_FAILURE);
        }
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
    if (with_system) {
        run_external_command("bash " + shell_single_quote((home / "scripts" / "init.sh").string()) +
                             " --refresh");
    }
    SUCCESS("Kez updated. Reload the shell to use the rebuilt executable.");
}
