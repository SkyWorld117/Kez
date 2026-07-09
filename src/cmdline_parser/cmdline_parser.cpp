#include <cmdline_parser/cmdline_parser.hpp>
#include <cmdline_parser/traverse.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <user_config_generator/user_config_generator.hpp>
#include <utils/bash_utils.hpp>
#include <utils/colored_io.hpp>
#include <utils/yaml_utils.hpp>

/**
 * @brief Apply command-line -D configuration overrides to a user config YAML
 *        node in-place.
 *
 * Iterates over a list of `-D`-style options (strings of the form
 * `KEY=VALUE`) and delegates each to traverse(), which walks the YAML tree
 * rooted at `user_config["kez"]` following the dot-separated path given by
 * the key.  Non-existent intermediate maps are created as needed; the final
 * leaf receives the value as a scalar.
 *
 * @param user_config    The root user-configuration YAML node.  Modified
 *                       in-place via the YAML::Node reference-counted handle.
 *                       Must contain a top-level `"kez"` key that is a map;
 *                       otherwise the program terminates.
 * @param config_options A vector of `KEY=VALUE` strings supplied on the
 *                       command line (e.g. `{"build.options.debug=true",
 *                       "build.parallel_jobs=8"}`).  Each key is a
 *                       dot-separated path interpreted by traverse().
 *
 * @warning Terminates the process with exit(EXIT_FAILURE) if
 *          `user_config["kez"]` is missing or is not a map, or if any
 *          config option lacks an `=` separator or has an empty key.
 *
 * @see traverse() for the path-resolution semantics inside the YAML tree.
 */
void apply_cmdline_config(YAML::Node user_config, const std::vector<std::string>& config_options) {
    if (!yaml_has(user_config, "kez") || !user_config["kez"].IsMap()) {
        ERROR("Invalid user configuration: kez must be a map");
        exit(EXIT_FAILURE);
    }

    for (const std::string& option : config_options) {
        const std::size_t separator = option.find('=');
        if (separator == std::string::npos || separator == 0) {
            ERROR("Invalid configuration override '" + option + "'; expected <path>=<value>");
            exit(EXIT_FAILURE);
        }
        traverse(option.substr(0, separator), option.substr(separator + 1), user_config["kez"]);
    }
}

/**
 * @brief Parse a user configuration YAML file on disk into an executable
 *        build plan.
 *
 * Checks that the file exists and is a regular file, loads it via
 * YAML::LoadFile(), then delegates to the YAML::Node overload of
 * parse_cmdline() which applies command-line overrides and parses the
 * configuration into a BashCommandPlan.
 *
 * @param file            Filesystem path to the user configuration YAML file
 *                        (e.g. the top-level config.yaml or a cluster preset).
 * @param install_prefix  Root directory under which all packages will be
 *                        installed.  Forwarded to parse_user_config().
 * @param config_options  Optional command-line -D overrides to apply on top
 *                        of the loaded YAML.  Defaults to an empty vector.
 *
 * @return A BashCommandPlan (vector of PackageCommands) containing the
 *         ordered list of packages to build, their shell commands, and
 *         inter-package dependencies.
 *
 * @warning Terminates the process with exit(EXIT_FAILURE) if @p file does
 *          not exist or is not a regular file.
 */
BashCommandPlan parse_cmdline(const std::filesystem::path& file,
                              const std::filesystem::path& install_prefix,
                              const std::vector<std::string>& config_options) {
    if (!std::filesystem::is_regular_file(file)) {
        ERROR("User configuration file does not exist: " + file.string());
        exit(EXIT_FAILURE);
    }
    return parse_cmdline(YAML::LoadFile(file.string()), install_prefix, config_options);
}

/**
 * @brief Generate and parse a build plan from a list of target package names.
 *
 * This overload is the entry point for ad-hoc `kez install <pkg>` invocations
 * where no pre-existing config file is involved.  It calls gen_user_config()
 * to resolve the dependency graph, generate a user configuration YAML tree,
 * and then delegates to the YAML::Node overload of parse_cmdline() for
 * override application and plan generation.
 *
 * The generator is invoked in non-interactive mode (second argument @c false),
 * meaning sensible defaults are chosen automatically rather than prompting
 * the user.
 *
 * @param targets         Non-empty vector of top-level package names (keys in
 *                        `database/<pkg>/latest.yaml`) to include in the plan.
 * @param install_prefix  Root directory under which all packages will be
 *                        installed.  Forwarded to parse_user_config().
 * @param config_options  Optional command-line -D overrides to apply on top
 *                        of the generated configuration.  Defaults to an
 *                        empty vector.
 *
 * @return A BashCommandPlan (vector of PackageCommands) with packages ordered
 *         topologically by their dependency graph.
 *
 * @warning Terminates the process with exit(EXIT_FAILURE) if @p targets is
 *          empty, or if gen_user_config() encounters missing packages,
 *          circular dependencies, or unsatisfiable configurations.
 */
BashCommandPlan parse_cmdline(const std::vector<std::string>& targets,
                              const std::filesystem::path& install_prefix,
                              const std::vector<std::string>& config_options) {
    if (targets.empty()) {
        ERROR("At least one target package is required");
        exit(EXIT_FAILURE);
    }
    return parse_cmdline(gen_user_config(targets, false), install_prefix, config_options);
}

/**
 * @brief Parse a pre-loaded YAML configuration node into an executable build
 *        plan, applying command-line overrides first.
 *
 * This is the central overload that both the file-based and target-list-based
 * parse_cmdline() variants delegate to.  It first applies any command-line
 * -D configuration overrides to the in-memory YAML node via
 * apply_cmdline_config(), then calls parse_user_config() to transform the
 * resolved configuration (including template expansion, uconf markers, and
 * external-package substitutions) into a BashCommandPlan.
 *
 * @param user_config    The user configuration as a YAML::Node.  Typically
 *                       originates from loading a YAML file from disk or
 *                       from gen_user_config().  Modified in-place during
 *                       override application.
 * @param install_prefix Root directory under which all packages will be
 *                       installed.  Forwarded to parse_user_config(),
 *                       which uses it to load the parser settings via
 *                       load_user_config_parser_settings().
 * @param config_options Optional command-line -D overrides applied by
 *                       apply_cmdline_config().  Defaults to an empty vector.
 *
 * @return A BashCommandPlan (vector of PackageCommands) ordered topologically
 *         by dependencies, ready for serialisation via write_install_plan().
 */
BashCommandPlan parse_cmdline(YAML::Node user_config, const std::filesystem::path& install_prefix,
                              const std::vector<std::string>& config_options) {
    apply_cmdline_config(user_config, config_options);
    return parse_user_config(user_config, install_prefix);
}

/**
 * @brief Serialise a BashCommandPlan to disk as a shell-sourced install plan.
 *
 * Writes the plan to the given file in a format consumed exclusively by
 * scripts/install.sh.  The file begins with a version header line
 * (`# kez-install-plan-v1`), followed by one stanza per package:
 *
 *   kez_plan_begin <package-name>
 *   kez_plan_depends <dependency-name>
 *   ...
 *   kez_plan_command <shell-command>
 *   ...
 *   kez_plan_end
 *
 * Each argument is single-quote-escaped via shell_single_quote() so that
 * the output is safe for shell eval.  After writing, the file's permissions
 * are set to owner-read/write only (0600).
 *
 * @param plan The build plan to serialise.  Each element is a
 *             PackageCommands struct containing the package name, its shell
 *             commands, and its dependency names.
 * @param path Filesystem path for the output file.  The parent directory is
 *             created if it does not exist.  An existing file is overwritten.
 *
 * @warning Terminates the process with exit(EXIT_FAILURE) if:
 *          - The parent directory cannot be created.
 *          - The output file cannot be opened for writing.
 *          - Writing fails (detected by checking the stream state after close).
 *          - The file permissions cannot be set to owner-read/write.
 *
 * @note The serialisation format is an internal protocol between the C++
 *       backend and scripts/install.sh.  Changing either side without
 *       updating the other will break the install pipeline.
 *
 * @see shell_single_quote() for the quoting mechanism used for each argument.
 */
void write_install_plan(const BashCommandPlan& plan, const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        ERROR("Failed to create installation plan directory: " + error.message());
        exit(EXIT_FAILURE);
    }

    std::ofstream output(path, std::ios::out | std::ios::trunc);
    if (!output) {
        ERROR("Failed to create installation plan: " + path.string());
        exit(EXIT_FAILURE);
    }

    output << "# kez-install-plan-v1\n";
    for (const PackageCommands& package : plan) {
        output << "kez_plan_begin " << shell_single_quote(package.package) << '\n';
        for (const std::string& dependency : package.dependencies) {
            output << "kez_plan_depends " << shell_single_quote(dependency) << '\n';
        }
        for (const std::string& command : package.commands) {
            output << "kez_plan_command " << shell_single_quote(command) << '\n';
        }
        output << "kez_plan_end\n";
    }
    output.close();
    if (!output) {
        ERROR("Failed to write installation plan: " + path.string());
        exit(EXIT_FAILURE);
    }

    std::filesystem::permissions(
        path, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
        std::filesystem::perm_options::replace, error);
    if (error) {
        ERROR("Failed to secure installation plan: " + error.message());
        exit(EXIT_FAILURE);
    }
}
