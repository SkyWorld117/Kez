/**
 * @file install.cpp
 * @brief Implementation of the `kez install` and `kez utilities` commands.
 *
 * This file implements the two top-level install pathways:
 *   - **install** – Installs packages into a named application environment.
 *     Supports generating a user config from package names, reading an existing
 *     YAML config (`--read`), dry-run mode, config-value overrides, Slurm
 *     submission, and the `--rebuild` workflow.
 *   - **utilities** – Manages the shared utilities environment via `add`
 *     (forwarding to the install pipeline) and `empty` (removing all packages).
 *
 * Both paths converge on run_install_plan(), which writes the parsed
 * BashCommandPlan to a temporary script and executes it through
 * scripts/install.sh.
 */

#include <unistd.h>
#include <yaml-cpp/yaml.h>

#include <cmdline_parser/cmdline_parser.hpp>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <parser/user_config_parser.hpp>
#include <rebuild/rebuild.hpp>
#include <string>
#include <system_error>
#include <ui/commands.hpp>
#include <ui/ui_utils.hpp>
#include <user_config_generator/user_config_generator.hpp>
#include <utils/bash_utils.hpp>
#include <utils/colored_io.hpp>
#include <vector>

namespace {
    /**
     * @brief Aggregated command-line options for the install / utilities add
     *        commands.
     *
     * Populated by parse_install_options() and consumed by the various install
     * subroutines.  Each field corresponds to a recognised CLI flag.
     */
    struct InstallOptions {
        /** @brief Treat the positional argument as a YAML file path. */
        bool read_file = false;

        /** @brief Print the install plan without executing it. */
        bool dry_run = false;

        /** @brief Reinstall packages already recorded in state.yaml. */
        bool force = false;

        /** @brief Run scripts/install.sh through sbatch (Slurm). */
        bool with_slurm = false;

        /** @brief Whether --rebuild was passed (activates the rebuild path). */
        bool rebuild = false;

        /**
         * @brief Name of the target environment (from `--env` or
         *        `KEZ_ACTIVE_ENV`).
         */
        std::string environment;

        /**
         * @brief Package to rebuild; set when `rebuild` is true (from
         *        `--rebuild`).
         */
        std::string rebuild_package;

        /**
         * @brief Config-value overrides from `--config` / `-c` in
         *        `PATH=VAL` form.
         */
        std::vector<std::string> overrides;

        /** @brief Unparsed positional arguments (package names or a file path). */
        std::vector<std::string> positional;
    };

    /**
     * @brief Print the help text for `kez install` or `kez utilities add`.
     *
     * Displays the usage synopsis and a list of recognised options to stdout.
     * Two variants are printed depending on whether the caller is the
     * application-install command (`kez install`) or the utilities command
     * (`kez utilities add`).
     *
     * @param utility  If true, print the `kez utilities add` help; otherwise
     *                 print the `kez install` help.
     */
    void print_install_help(bool utility) {
        if (utility) {
            std::cout
                << "Usage: kez utilities add [options] <package>...\n\n"
                   "Options:\n"
                   "  -r, --read             Treat the positional argument as a YAML file\n"
                   "  -d, --dry-run          Show the commands that would be executed\n"
                   "  -c, --config PATH=VAL  Override a generated configuration value\n"
                   "  -f, --force            Reinstall packages already recorded in state.yaml\n"
                   "  -S, --with-slurm       Run scripts/install.sh through sbatch\n"
                   "  -R,  --rebuild PACKAGE  Rebuild a package and its dependents in the env\n";
        } else {
            std::cout
                << "Usage: kez install [options] <package>...\n"
                   "       kez install --read [options] <config.yaml>\n\n"
                   "Options:\n"
                   "  -r, --read             Treat the positional argument as a YAML file\n"
                   "  -d, --dry-run          Show the commands that would be executed\n"
                   "  -c, --config PATH=VAL  Override a generated configuration value\n"
                   "  -e, --env NAME         Target application environment\n"
                   "  -f, --force            Reinstall packages already recorded in state.yaml\n"
                   "  -S, --with-slurm       Run scripts/install.sh through sbatch\n"
                   "      --rebuild PACKAGE  Rebuild a package and its dependents in the env\n";
        }
    }

    /**
     * @brief Extract the next positional argument as a required option value.
     *
     * Advances the index and returns the token at the new position.  If no
     * further tokens exist, the program terminates with an error.
     *
     * @param arguments  The full list of command-line tokens.
     * @param index      Current position in @p arguments; incremented by one on
     *                   success.
     * @param option     The option name that requires a value (used in the error
     *                   message, e.g. "--rebuild").
     *
     * @return The value of the required option (the token following @p option).
     *
     * @warning Terminates the process via ERROR() and exit(EXIT_FAILURE) when
     *          there is no next token.
     */
    std::string required_value(const CommandArguments& arguments, std::size_t& index,
                               const std::string& option) {
        if (index + 1 >= arguments.size()) {
            ERROR("Missing value for " + option);
            exit(EXIT_FAILURE);
        }
        return arguments[++index];
    }

    /**
     * @brief Parse command-line arguments into an InstallOptions struct.
     *
     * Iterates over the argument vector once, recognising short and long forms
     * of all supported flags.  After a bare `--` token, all remaining arguments
     * are treated as positional values.  Unknown flags prefixed with `-` cause
     * immediate termination.
     *
     * @param arguments  The command-line tokens to parse (excluding the leading
     *                   subcommand name, e.g. "install" or "utilities").
     * @param utility    If true, certain flags (`--env`, `--rebuild`) are
     *                   rejected as invalid for utility installation.
     * @param help       [out] Set to true if `-h` or `--help` was encountered.
     *
     * @return A fully populated InstallOptions reflecting the parsed flags and
     *         positional arguments.
     *
     * @warning Terminates the process if `--rebuild` or `--env` is used in
     *          utility mode, or if an unknown option is encountered.
     */
    InstallOptions parse_install_options(const CommandArguments& arguments, bool utility,
                                         bool& help) {
        InstallOptions result;
        bool positional_only = false;
        for (std::size_t index = 0; index < arguments.size(); ++index) {
            const std::string& argument = arguments[index];
            if (positional_only) {
                result.positional.push_back(argument);
            } else if (argument == "--") {
                positional_only = true;
            } else if (argument == "-h" || argument == "--help") {
                help = true;
            } else if (argument == "-r" || argument == "--read") {
                result.read_file = true;
            } else if (argument.rfind("--read=", 0) == 0) {
                result.read_file = true;
                result.positional.push_back(argument.substr(7));
            } else if (argument == "-d" || argument == "--dry-run") {
                result.dry_run = true;
            } else if (argument == "-f" || argument == "--force") {
                result.force = true;
            } else if (argument == "-S" || argument == "--with-slurm") {
                result.with_slurm = true;
            } else if (argument == "-R" || argument == "--rebuild") {
                if (utility) {
                    ERROR("--rebuild is not valid for utility installation");
                    exit(EXIT_FAILURE);
                }
                result.rebuild         = true;
                result.rebuild_package = required_value(arguments, index, argument);
            } else if (argument.rfind("--rebuild=", 0) == 0) {
                if (utility) {
                    ERROR("--rebuild is not valid for utility installation");
                    exit(EXIT_FAILURE);
                }
                result.rebuild         = true;
                result.rebuild_package = argument.substr(10);
            } else if (argument == "-e" || argument == "--env") {
                if (utility) {
                    ERROR(argument + " is not valid for utility installation");
                    exit(EXIT_FAILURE);
                }
                result.environment = required_value(arguments, index, argument);
            } else if (argument.rfind("--env=", 0) == 0) {
                if (utility) {
                    ERROR("--env is not valid for utility installation");
                    exit(EXIT_FAILURE);
                }
                result.environment = argument.substr(6);
            } else if (argument == "-c" || argument == "--config") {
                result.overrides.push_back(required_value(arguments, index, argument));
                // Greedily consume subsequent tokens that look like key=value
                // pairs (non-empty, contain '=', do not start with '-').
                while (index + 1 < arguments.size() && !arguments[index + 1].empty() &&
                       arguments[index + 1].find('=') != std::string::npos &&
                       arguments[index + 1].front() != '-') {
                    result.overrides.push_back(arguments[++index]);
                }
            } else if (argument.rfind("--config=", 0) == 0) {
                result.overrides.push_back(argument.substr(9));
            } else if (!argument.empty() && argument.front() == '-') {
                ERROR("Unknown install option: " + argument);
                exit(EXIT_FAILURE);
            } else {
                result.positional.push_back(argument);
            }
        }
        return result;
    }

    /**
     * @brief Load or generate the user configuration YAML from the parsed
     *        options.
     *
     * When `--read` was specified, the function validates that exactly one
     * positional argument (a file path) was given, checks that the file exists,
     * and loads it via YAML::LoadFile().  Otherwise it calls gen_user_config()
     * to generate a configuration from the positional package names.
     *
     * @param options  The parsed install options (must contain at least one
     *                 positional argument).
     *
     * @return A YAML::Node representing the user configuration.
     *
     * @warning Terminates the process if no positional argument was provided,
     *          if `--read` was given with zero or more than one argument, or
     *          if the config file does not exist.
     *
     * @see gen_user_config
     */
    YAML::Node load_install_config(const InstallOptions& options) {
        if (options.positional.empty()) {
            ERROR("No package or configuration file was provided");
            exit(EXIT_FAILURE);
        }
        if (options.read_file) {
            if (options.positional.size() != 1) {
                ERROR("--read accepts exactly one user configuration file");
                exit(EXIT_FAILURE);
            }
            const std::filesystem::path path = options.positional.front();
            if (!std::filesystem::is_regular_file(path)) {
                ERROR("User configuration file does not exist: " + path.string());
                exit(EXIT_FAILURE);
            }
            return YAML::LoadFile(path.string());
        }
        return gen_user_config(options.positional, false);
    }

    /**
     * @brief Resolve the installation prefix for a named application
     *        environment.
     *
     * If @p environment is non-empty, it is used directly; otherwise the
     * function falls back to the `KEZ_ACTIVE_ENV` environment variable.  The
     * resulting name is validated as a safe filesystem path component and then
     * joined under the configured work path's `applications/` subdirectory.
     *
     * @param environment  The environment name, or an empty string to use
     *                     KEZ_ACTIVE_ENV.
     *
     * @return An absolute path `<workdir>/applications/<name>`.
     *
     * @warning Terminates the process if both @p environment and
     *          KEZ_ACTIVE_ENV are empty, or if the resolved name contains
     *          unsafe characters (slashes, null bytes, whitespace).
     *
     * @see configured_work_path
     * @see validate_path_component
     */
    std::filesystem::path resolve_application_prefix(const std::string& environment) {
        std::string selected = environment;
        if (selected.empty()) {
            selected = get_env_var_noerr("KEZ_ACTIVE_ENV");
        }
        if (selected.empty()) {
            ERROR("A target environment is required; pass --env <name> or run 'kez env enter "
                  "<name>'");
            exit(EXIT_FAILURE);
        }
        validate_path_component(selected, "environment name");
        return configured_work_path("applications") / selected;
    }

    /**
     * @brief Write an install plan to a temporary file and execute it via
     *        scripts/install.sh.
     *
     * Creates the `.tmp` directory inside @p prefix, serialises the plan to a
     * unique temporary script (named with the current PID), constructs a shell
     * command that invokes scripts/install.sh with the prefix and plan path,
     * and runs it.  The temporary file is removed after execution completes.
     *
     * The `KEZ_INSTALL_JOBS` environment variable is set from the parser
     * settings; its value can be overridden by the `KEZ_INSTALL_JOBS`
     * environment variable if already present in the calling environment.
     *
     * @param prefix          The installation prefix (environment root).
     * @param plan            The parsed BashCommandPlan to execute.
     * @param parser_settings Settings providing the parallel-jobs count and
     *                        other install parameters.
     * @param force           If true, the `--force` flag is forwarded to
     *                        install.sh, causing already-recorded packages to
     *                        be reinstalled.
     * @param with_slurm      If true, the install command is wrapped in
     *                        `sbatch --wait` for Slurm submission.
     *
     * @warning Terminates the process if the `.tmp` directory cannot be
     *          created, if scripts/install.sh does not exist, or if the
     *          executed command returns a non-zero exit status (handled by
     *          run_external_command).
     *
     * @see write_install_plan
     * @see run_external_command
     */
    void run_install_plan(const std::filesystem::path& prefix, const BashCommandPlan& plan,
                          const UserConfigParserSettings& parser_settings, bool force,
                          bool with_slurm) {
        std::error_code error;
        std::filesystem::create_directories(prefix / ".tmp", error);
        if (error) {
            ERROR("Failed to create installation environment: " + error.message());
            exit(EXIT_FAILURE);
        }

        const std::filesystem::path plan_path =
            prefix / ".tmp" / ("install-plan-" + std::to_string(getpid()) + ".sh");
        write_install_plan(plan, plan_path);

        const std::filesystem::path script =
            std::filesystem::path(get_env_var("KEZ_HOME")) / "scripts" / "install.sh";
        if (!std::filesystem::is_regular_file(script)) {
            ERROR("Installation executor does not exist: " + script.string());
            exit(EXIT_FAILURE);
        }

        const std::string install_jobs =
            get_env_var_noerr("KEZ_INSTALL_JOBS", std::to_string(parser_settings.parallel_jobs));
        std::string command = "KEZ_INSTALL_JOBS=" + shell_single_quote(install_jobs) + " bash " +
                              shell_single_quote(script.string()) + " " +
                              shell_single_quote(prefix.string()) + " " +
                              shell_single_quote(plan_path.string());
        if (force) {
            command += " --force";
        }
        if (with_slurm) {
            command = "sbatch --wait --job-name=kez-install --wrap=" + shell_single_quote(command);
        }
        run_external_command(command);
        std::filesystem::remove(plan_path, error);
        if (error) {
            WARNING("Could not remove installation plan: " + error.message());
        }
    }

    /**
     * @brief Rebuild a package and its transitive dependents within an
     *        existing environment.
     *
     * This is the implementation of the `--rebuild <pkg>` / `-R <pkg>` flag.
     * Validates that:
     *   1. `--read` was not also specified (mutually exclusive).
     *   2. No positional package arguments were given (the rebuild set is
     *      derived from the installed state, not user-provided targets).
     *   3. A rebuild package name was supplied.
     *   4. The target environment exists.
     *   5. The target package is recorded in the environment's state.yaml.
     *
     * On success, it loads the full set of installed packages, regenerates a
     * configuration for all of them, parses it, computes the transitive closure
     * of dependents of the target package via compute_rebuild_set(), and either
     * prints the filtered plan (dry-run) or executes it.  The `--force` flag is
     * always set when running the filtered plan because every member is already
     * recorded in state.yaml.
     *
     * @param options  The parsed install options; must have `rebuild` true and
     *                 `rebuild_package` non-empty.
     *
     * @warning Terminates the process on any validation failure, if the
     *          environment does not exist, if the target package is not
     *          installed, or if plan generation/parsing fails.
     *
     * @see compute_rebuild_set
     * @see filter_plan
     * @see run_install_plan
     */
    void rebuild(const InstallOptions& options) {
        if (options.read_file) {
            ERROR("--rebuild cannot be combined with --read");
            exit(EXIT_FAILURE);
        }
        if (!options.positional.empty()) {
            ERROR("--rebuild does not accept package arguments");
            exit(EXIT_FAILURE);
        }
        if (options.rebuild_package.empty()) {
            ERROR("--rebuild requires a package name");
            exit(EXIT_FAILURE);
        }

        const std::filesystem::path prefix = resolve_application_prefix(options.environment);
        if (!std::filesystem::is_directory(prefix)) {
            ERROR("Environment does not exist: " + prefix.string());
            exit(EXIT_FAILURE);
        }

        const std::vector<std::string> installed = load_installed_packages(prefix);
        if (std::find(installed.begin(), installed.end(), options.rebuild_package) ==
            installed.end()) {
            ERROR("Package '" + options.rebuild_package + "' is not installed in " +
                  prefix.string());
            exit(EXIT_FAILURE);
        }

        const YAML::Node user_config            = gen_user_config(installed, false);
        const UserConfigParserSettings settings = load_user_config_parser_settings(prefix);
        const BashCommandPlan plan              = parse_user_config(user_config, settings);

        const std::vector<std::string> rebuild_set =
            compute_rebuild_set(plan, options.rebuild_package);
        std::string rebuild_list;
        for (const std::string& package : rebuild_set) {
            rebuild_list += (rebuild_list.empty() ? "" : " ") + package;
        }
        INFO("Rebuilding: " + rebuild_list);
        const BashCommandPlan filtered = filter_plan(plan, rebuild_set);

        if (options.dry_run) {
            print_command_plan(filtered);
            return;
        }

        // --force is required: every rebuild-set member is already recorded in state.yaml and
        // would otherwise be skipped. The filtered plan contains only the rebuild set.
        run_install_plan(prefix, filtered, settings, true, options.with_slurm);
    }

    /**
     * @brief Main install routine: parse options, load/generate config, and
     *        execute the install plan.
     *
     * This is the shared implementation for both `kez install` and
     * `kez utilities add`.  The flow is:
     *   1. Parse command-line arguments into InstallOptions.
     *   2. If `--help` was requested, print help and return.
     *   3. If `--rebuild` was given, delegate to rebuild() and return.
     *   4. Load or generate the user configuration (load_install_config()).
     *   5. Apply any `--config` YAML overrides.
     *   6. Resolve the installation prefix from the environment name.
     *   7. Load parser settings from the prefix.
     *   8. Parse the config into a BashCommandPlan.
     *   9. Print the plan (dry-run) or execute it via run_install_plan().
     *
     * @param arguments  The command-line tokens after the subcommand name.
     * @param utility    If true, the call originated from `kez utilities add`;
     *                   influences help text and option validation.
     *
     * @see parse_install_options
     * @see load_install_config
     * @see installation_prefix
     * @see run_install_plan
     */
    void install(const CommandArguments& arguments, bool utility) {
        bool help                    = false;
        const InstallOptions options = parse_install_options(arguments, utility, help);
        if (help) {
            print_install_help(utility);
            return;
        }

        if (options.rebuild) {
            rebuild(options);
            return;
        }

        YAML::Node user_config = load_install_config(options);
        apply_cmdline_config(user_config, options.overrides);
        const std::filesystem::path prefix =
            installation_prefix(user_config, options.environment, utility);

        const UserConfigParserSettings parser_settings = load_user_config_parser_settings(prefix);
        const BashCommandPlan plan = parse_user_config(user_config, parser_settings);
        if (options.dry_run) {
            print_command_plan(plan);
            return;
        }

        run_install_plan(prefix, plan, parser_settings, options.force, options.with_slurm);
    }

    /**
     * @brief Remove all installed packages from the utilities environment.
     *
     * Iterates over every entry in `configured_work_path("utilities")` and
     * deletes it recursively.  If the utilities directory does not exist at
     * all, an informational message is printed and the function returns
     * without error.
     *
     * @warning Terminates the process if any individual removal fails (the
     *          directory iterator encounters an error from remove_all).
     *
     * @see configured_work_path
     */
    void empty_utilities() {
        const std::filesystem::path root = configured_work_path("utilities");
        if (!std::filesystem::is_directory(root)) {
            INFO("Utilities environment does not exist; nothing to empty.");
            return;
        }
        std::error_code error;
        for (const auto& entry : std::filesystem::directory_iterator(root)) {
            std::filesystem::remove_all(entry.path(), error);
            if (error) {
                ERROR("Failed to empty utilities environment: " + error.message());
                exit(EXIT_FAILURE);
            }
        }
        SUCCESS("Utilities environment emptied.");
    }
}  // namespace

/**
 * @brief Entry point for the `kez install` command.
 *
 * Delegates to the internal install() function with `utility = false`,
 * which runs the full install pipeline: option parsing, config generation
 * or loading, plan parsing, and execution.
 *
 * @param arguments  Command-line tokens after the `install` subcommand.
 *
 * @see execute_utilities
 * @see install
 */
void execute_install(const CommandArguments& arguments) { install(arguments, false); }

/**
 * @brief Entry point for the `kez utilities` command.
 *
 * Dispatches to one of three paths:
 *   - **No arguments / `-h` / `--help`**: Print usage and return.
 *   - **`empty`**: Delete all packages from the utilities environment
 *     (delegates to empty_utilities()).  No additional arguments are
 *     accepted.
 *   - **`add`**: Forward the remaining arguments to the install pipeline
 *     with `utility = true`, which installs packages into the shared
 *     utilities directory.
 *
 * @param arguments  Command-line tokens after the `utilities` subcommand.
 *                   The first token must be `add`, `empty`, or absent
 *                   (triggers help).
 *
 * @warning Terminates the process if the first token is not `add`, `empty`,
 *          or a help flag, or if `empty` is given with additional arguments.
 *
 * @see execute_install
 * @see empty_utilities
 * @see install
 */
void execute_utilities(const CommandArguments& arguments) {
    if (arguments.empty() || arguments.front() == "-h" || arguments.front() == "--help") {
        std::cout << "Usage: kez utilities <add|empty> [options]\n";
        return;
    }
    if (arguments.front() == "empty") {
        if (arguments.size() != 1) {
            ERROR("utilities empty does not accept additional arguments");
            exit(EXIT_FAILURE);
        }
        empty_utilities();
        return;
    }
    if (arguments.front() != "add") {
        ERROR("Unknown utilities command: " + arguments.front());
        exit(EXIT_FAILURE);
    }
    install(CommandArguments(arguments.begin() + 1, arguments.end()), true);
}
