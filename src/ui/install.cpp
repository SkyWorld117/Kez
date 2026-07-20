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
 *     and `empty` (removing all packages).
 *
 * Both paths converge on run_install_plan(), which writes the parsed
 * BashCommandPlan to a temporary script and executes it through
 * scripts/install.sh.
 */

#include <unistd.h>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cmdline_parser/cmdline_parser.hpp>
#include <cstdlib>
#include <filesystem>
#include <rebuild/rebuild.hpp>
#include <string>
#include <system_error>
#include <uconf_generator/uconf_generator.hpp>
#include <uconf_parser/user_config_parser.hpp>
#include <ui/argparse.hpp>
#include <ui/commands.hpp>
#include <ui/install.hpp>
#include <ui/ui_utils.hpp>
#include <unordered_map>
#include <utils/bash_utils.hpp>
#include <utils/colored_io.hpp>
#include <utils/file_utils.hpp>
#include <utils/yaml_utils.hpp>
#include <vector>

std::string install_executor_command(const std::filesystem::path& executor,
                                     const std::filesystem::path& prefix,
                                     const std::filesystem::path& plan_path, bool force,
                                     bool with_slurm, const std::string& slurm_job) {
    std::string command = "bash " + shell_single_quote(executor.string()) + " " +
                          shell_single_quote(prefix.string()) + " " +
                          shell_single_quote(plan_path.string());
    if (force) {
        command += " --force";
    }
    if (with_slurm) {
        command =
            "sbatch --wait --job-name=" + slurm_job + " --wrap=" + shell_single_quote(command);
    }
    return command;
}

RebuildPlanSelection select_rebuild_plan(const BashCommandPlan& plan, const std::string& target) {
    RebuildPlanSelection result;
    result.target_found = std::any_of(
        plan.begin(), plan.end(),
        [&target](const PackageCommands& package) { return package.package == target; });
    if (!result.target_found) {
        return result;
    }
    result.packages = compute_rebuild_set(plan, target);
    result.plan     = filter_plan(plan, result.packages);
    return result;
}

YAML::Node state_without_package(const YAML::Node& state, const std::string& package) {
    YAML::Node result(YAML::NodeType::Sequence);
    if (!state.IsSequence()) {
        return result;
    }
    for (const YAML::Node& entry : state) {
        if (entry.IsScalar() && entry.Scalar() != package) {
            result.push_back(entry);
        }
    }
    return result;
}

namespace {
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
            INFO("Usage: kez utilities add [options] <package>...\n\n"
                 "Options:\n"
                 "  -r, --read             Treat the positional argument as a YAML file\n"
                 "  -d, --dry-run          Show the commands that would be executed\n"
                 "  -c, --config PATH=VAL  Override a generated configuration value\n"
                 "  -f, --force            Reinstall packages already recorded in state.yaml\n"
                 "  -S, --with-slurm       Run scripts/install.sh through sbatch\n"
                 "      --silence          Generate configuration without prompting");
        } else {
            INFO("Usage: kez install [options] <package>...\n"
                 "       kez install --read [options] <config.yaml>\n\n"
                 "Options:\n"
                 "  -r, --read             Treat the positional argument as a YAML file\n"
                 "  -d, --dry-run          Show the commands that would be executed\n"
                 "  -c, --config PATH=VAL  Override a generated configuration value\n"
                 "  -e, --env NAME         Target application environment\n"
                 "  -f, --force            Reinstall packages already recorded in state.yaml\n"
                 "  -S, --with-slurm       Run scripts/install.sh through sbatch\n"
                 "      --silence          Generate configuration without prompting\n"
                 "      --rename NAME      Rename the version in a compiler/MPI/vendor prefix\n"
                 "  -R, --rebuild PACKAGE  Rebuild a package and its dependents in the env\n"
                 "                         (may be combined with --read)");
        }
    }

    /**
     * @brief Load or generate the user configuration YAML node.
     *
     * If --read was supplied, loads the named file; otherwise generates a
     * config from the positional package-name arguments.
     */
    /**
     * @brief Extract version overrides from command-line --config options.
     *
     * Looks for options matching the pattern ``<package>.version=<value>``
     * and returns a map of package name → version string (e.g.
     * ``"conquest" → "1.4"``).
     */
    std::unordered_map<std::string, std::string> extract_version_overrides(
        const std::vector<std::string>& overrides) {
        std::unordered_map<std::string, std::string> result;
        static const std::string suffix = ".version=";
        for (const std::string& option : overrides) {
            const std::size_t pos = option.find(suffix);
            if (pos != std::string::npos && pos > 0) {
                result.emplace(option.substr(0, pos), option.substr(pos + suffix.size()));
            }
        }
        return result;
    }

    YAML::Node load_install_config(
        const InstallOptions& options,
        const std::unordered_map<std::string, std::string>& version_overrides) {
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
            return load_yaml_file(path);
        }
        return gen_user_config(options.positional, !options.silent, version_overrides);
    }

    YAML::Node load_install_config(const InstallOptions& options) {
        return load_install_config(options, {});
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
            ERROR("A target environment is required; pass --env <name> or run 'kez env activate "
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
                          bool force, bool with_slurm) {
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

        run_external_command(
            install_executor_command(script, prefix, plan_path, force, with_slurm, "kez-install"));
        std::filesystem::remove(plan_path, error);
        if (error) {
            WARNING("Could not remove installation plan: " + error.message());
        }
    }

    /**
     * @brief Rebuild a single package and its transitive dependents.
     *
     * Two modes:
     *
     *   1. **Without `--read`** (default) — loads the installed package list
     *      from the target environment's ``state.yaml``, regenerates a fresh
     *      user config from those names, and rebuilds the target + dependents.
     *      The target must already be installed in the environment.
     *
     *   2. **With `--read <config.yaml>`** — loads the user's custom config
     *      file instead, applies any ``--config`` overrides, and rebuilds the
     *      target + dependents according to that config.  The target must
     *      appear somewhere in the parsed plan (not necessarily as a top-level
     *      target in the config).
     *
     * In both modes the plan is narrowed to the transitive dependent closure
     * of the target and the install script is run with ``--force`` so that
     * already-recorded packages are reinstalled.
     *
     * @param options  Parsed command-line options.  `rebuild_package` must be
     *                 set; when `read_file` is true, `positional` must contain
     *                 exactly one path (the config file).
     * @param utility  Whether this is a utility install (always false for
     *                 rebuild, as the flag is rejected earlier).
     */
    void rebuild(const InstallOptions& options, bool utility) {
        if (options.rebuild_package.empty()) {
            ERROR("--rebuild requires a package name");
            exit(EXIT_FAILURE);
        }
        if (!options.renamed_version.empty() && !options.read_file) {
            ERROR("--rename requires --read when used with --rebuild");
            exit(EXIT_FAILURE);
        }

        YAML::Node user_config;
        std::filesystem::path prefix;

        if (options.read_file) {
            // Mode 2: use the user-provided config
            user_config = load_install_config(options);
            apply_cmdline_config(user_config, options.overrides);
            prefix = installation_prefix(user_config, options.environment, utility,
                                         options.renamed_version);
        } else {
            // Mode 1: regenerate from installed packages
            if (!options.positional.empty()) {
                ERROR("--rebuild does not accept package arguments");
                exit(EXIT_FAILURE);
            }
            prefix = resolve_application_prefix(options.environment);
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
            user_config = gen_user_config(installed, !options.silent);
        }

        UserConfigParserSettings settings              = load_user_config_parser_settings(prefix);
        settings.use_install_prefix_for_managed_target = !options.renamed_version.empty();
        const BashCommandPlan plan                     = parse_user_config(user_config, settings);

        const RebuildPlanSelection selection = select_rebuild_plan(plan, options.rebuild_package);
        if (!selection.target_found) {
            ERROR("Package '" + options.rebuild_package + "' is not part of the install plan");
            exit(EXIT_FAILURE);
        }

        std::string rebuild_list;
        for (const std::string& package : selection.packages) {
            rebuild_list += (rebuild_list.empty() ? "" : " ") + package;
        }
        INFO("Rebuilding: " + rebuild_list);
        const BashCommandPlan& filtered = selection.plan;

        if (options.dry_run) {
            print_command_plan(filtered);
            return;
        }

        // --force is required: every rebuild-set member is already recorded in
        // state.yaml and would otherwise be skipped.
        run_install_plan(prefix, filtered, true, options.with_slurm);
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
        const InstallOptionsParseResult parsed = parse_install_options(arguments, utility);
        if (!parsed.error.empty()) {
            ERROR(parsed.error);
            exit(EXIT_FAILURE);
        }
        if (parsed.help) {
            print_install_help(utility);
            return;
        }
        const InstallOptions& options = parsed.options;

        if (options.rebuild) {
            rebuild(options, utility);
            return;
        }

        const auto version_overrides = extract_version_overrides(options.overrides);
        YAML::Node user_config       = version_overrides.empty()
                                           ? load_install_config(options)
                                           : load_install_config(options, version_overrides);
        apply_cmdline_config(user_config, options.overrides);
        const std::filesystem::path prefix =
            installation_prefix(user_config, options.environment, utility, options.renamed_version);

        UserConfigParserSettings parser_settings = load_user_config_parser_settings(prefix);
        parser_settings.use_install_prefix_for_managed_target = !options.renamed_version.empty();
        const BashCommandPlan plan = parse_user_config(user_config, parser_settings);
        if (options.dry_run) {
            print_command_plan(plan);
            return;
        }

        run_install_plan(prefix, plan, options.force, options.with_slurm);
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
     * @brief Remove a single package from the shared utilities environment.
     *
     * Validates the package name, deletes its directory tree under
     * <work>/utilities/, and removes the package from state.yaml so that
     * subsequent operations (e.g. a rebuild) do not reference a
     * half-removed package.
     *
     * PATH cleanup is handled by the shell wrapper in main.sh after the
     * binary returns, so this function only performs filesystem operations.
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
            YAML::Node document = load_yaml_file(state_file);

            if (yaml_has(document, "state") && document["state"].IsSequence()) {
                document["state"] = state_without_package(document["state"], package);
                write_yaml_atomic(document, state_file.string());
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
    const UtilitiesArgumentsParseResult parsed = parse_utilities_arguments(arguments);
    if (!parsed.error.empty()) {
        ERROR(parsed.error);
        exit(EXIT_FAILURE);
    }
    switch (parsed.arguments.action) {
        case UtilitiesAction::Help:
            INFO("Usage: kez utilities <add|remove|empty> [options]");
            return;
        case UtilitiesAction::Add: install(parsed.arguments.install_arguments, true); return;
        case UtilitiesAction::Remove: remove_utilities_package(parsed.arguments.package); return;
        case UtilitiesAction::Empty: empty_utilities(); return;
    }
}
