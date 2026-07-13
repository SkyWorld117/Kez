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
 *     (forwarding to the install pipeline), `remove` (deleting a single package),
 *     `reload` (updating PATH), and `empty` (removing all packages).
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
#include <utils/file_utils.hpp>
#include <utils/string_utils.hpp>
#include <utils/yaml_utils.hpp>
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
     * @brief Retrieve the next positional argument as the value for a flag.
     *
     * Exits the process if there is no next argument.
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
     * Handles short and long flags, greedy consumption of key=value overrides
     * after --config / -c, and the `--` positional-only separator.
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
     * @brief Load or generate the user configuration YAML node.
     *
     * If --read was supplied, loads the named file; otherwise generates a
     * config from the positional package-name arguments.
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
     * @brief Resolve the application install prefix from --env or KEZ_ACTIVE_ENV.
     *
     * Validates the environment name and returns
     * <work>/applications/<environment>.
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
     * @brief Write the parsed plan to a temporary file and execute it via
     *        scripts/install.sh.
     *
     * Creates the .tmp directory under the prefix, serialises the plan,
     * optionally wraps the invocation in sbatch for Slurm, and cleans up the
     * temporary file on completion.
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
     * @brief Rebuild a single package and its transitive dependents.
     *
     * Loads the installed package list for the target environment, generates a
     * fresh user config, computes the rebuild set from the dependency graph,
     * and re-runs the install plan confined to that set.
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
     * @brief Top-level install dispatcher.
     *
     * Parses options, loads or generates a user config, applies command-line
     * overrides, resolves the installation prefix, parses the plan, and either
     * prints it (dry-run) or executes it.  Delegates to rebuild() when
     * --rebuild is active.
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
     * @brief Remove all packages from the shared utilities environment.
     *
     * Iterates over every entry under <work>/utilities and deletes it.
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

    /**
     * @brief Emit shell commands to prepend each utility package's bin/
     *        directory to PATH.
     *
     * Scans every subdirectory under <work>/utilities/ and, for each package
     * that has a bin/ subdirectory, emits a guarded ``export PATH=...`` command
     * that prepends it to PATH only if it is not already present.  The output
     * is intended to be evaluated by the calling shell via the wrapper in
     * main.sh (``kez utilities reload``).
     *
     * This mirrors the logic in setup-env.sh that initialises PATH from the
     * utilities tree at shell-login time.
     */
    void reload_utilities() {
        const std::filesystem::path root = configured_work_path("utilities");
        if (!std::filesystem::is_directory(root)) {
            INFO("Utilities environment does not exist; nothing to reload.");
            return;
        }

        std::vector<std::string> bin_dirs;
        for (const auto& entry : std::filesystem::directory_iterator(root)) {
            if (!entry.is_directory()) continue;
            const std::string name = entry.path().filename().string();
            if (name.empty() || name[0] == '.') continue;

            const auto bin_dir = entry.path() / "bin";
            if (std::filesystem::is_directory(bin_dir)) {
                bin_dirs.push_back(bin_dir.string());
            }
        }

        if (bin_dirs.empty()) {
            INFO("No utility packages with bin/ directories found.");
            return;
        }

        // Emit a single export PATH that prepends all collected bin directories.
        // This follows the same pattern as emit_environment_activation() — no
        // duplicate-guard, as the wrapper in main.sh evaluates the output into
        // the current shell and the user is expected to call reload sparingly.
        // The paths are single-quoted for safe shell evaluation.
        std::cout << "export PATH=" << shell_single_quote(join(bin_dirs, ":")) << ":\"${PATH}\";\n";
    }

    /**
     * @brief Remove a single package from the shared utilities environment.
     *
     * Validates the package name, deletes its directory tree under
     * <work>/utilities/, and removes the package from state.yaml so that
     * subsequent operations (e.g. a rebuild) do not reference a
     * half-removed package.
     *
     * @param package  The name of the utility package to remove.
     *
     * @warning Terminates the process with EXIT_FAILURE if the package is not
     *          installed or cannot be deleted.
     */
    void remove_utilities_package(const std::string& package) {
        validate_path_component(package, "utility package name");

        const std::filesystem::path root     = configured_work_path("utilities");
        const std::filesystem::path pkg_path = root / package;

        if (!std::filesystem::is_directory(pkg_path)) {
            ERROR("Utility package is not installed: " + package);
            exit(EXIT_FAILURE);
        }

        // Remove the package directory tree.
        std::error_code error;
        std::filesystem::remove_all(pkg_path, error);
        if (error) {
            ERROR("Failed to remove utility package '" + package + "': " + error.message());
            exit(EXIT_FAILURE);
        }

        // Remove the package from state.yaml, if it exists.
        const std::filesystem::path state_file = root / "state.yaml";
        if (std::filesystem::is_regular_file(state_file)) {
            YAML::Node document;
            try {
                document = YAML::LoadFile(state_file.string());
            } catch (const YAML::Exception& err) {
                ERROR("Failed to parse utilities state file: " + state_file.string() + "\n" +
                      err.what());
                exit(EXIT_FAILURE);
            }

            if (yaml_has(document, "state") && document["state"].IsSequence()) {
                YAML::Node updated;
                updated["state"] = YAML::Node(YAML::NodeType::Sequence);

                for (const YAML::Node& entry : document["state"]) {
                    if (!entry.IsScalar()) continue;
                    if (entry.as<std::string>() == package) continue;
                    updated["state"].push_back(entry);
                }

                write_yaml(updated, state_file.string());
            }
        }

        SUCCESS("Utility package removed: " + package);
    }
}  // namespace

/**
 * @brief Entry point for `kez install` -- delegates to the internal install()
 *        helper in application (non-utility) mode.
 */
void execute_install(const CommandArguments& arguments) { install(arguments, false); }

/**
 * @brief Entry point for `kez utilities` -- dispatches to empty_utilities()
 *        or forward to install() with utility=true for the `add` subcommand.
 */
void execute_utilities(const CommandArguments& arguments) {
    if (arguments.empty() || arguments.front() == "-h" || arguments.front() == "--help") {
        std::cout << "Usage: kez utilities <add|remove|reload|empty> [options]\n";
        return;
    }
    if (arguments.front() == "remove") {
        if (arguments.size() != 2) {
            ERROR("utilities remove requires exactly one package name");
            exit(EXIT_FAILURE);
        }
        remove_utilities_package(arguments[1]);
        return;
    }
    if (arguments.front() == "reload") {
        if (arguments.size() != 1) {
            ERROR("utilities reload does not accept additional arguments");
            exit(EXIT_FAILURE);
        }
        reload_utilities();
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
