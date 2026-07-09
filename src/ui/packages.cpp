#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cstdlib>
#include <database/config.hpp>
#include <database/config_selector.hpp>
#include <database/database.hpp>
#include <filesystem>
#include <iostream>
#include <parser/user_config_parser.hpp>
#include <string>
#include <ui/commands.hpp>
#include <ui/ui_utils.hpp>
#include <user_config_generator/user_config_generator.hpp>
#include <utils/bash_utils.hpp>
#include <utils/colored_io.hpp>
#include <utils/file_utils.hpp>
#include <variant>
#include <vector>

namespace {
    /**
     * @brief Converts a Toolchain enum to its human-readable string representation.
     *
     * Maps each Toolchain enumerator to the corresponding string used in package
     * metadata and user-facing output. The returned string is the canonical name
     * for the build-system toolchain that a package uses.
     *
     * @param toolchain  The Toolchain enumerator to convert.
     * @return std::string  "none", "autotools", "cmake", or "make".  If the
     *                      enumerator does not match any known case, returns
     *                      "unknown".

     * @warning If a new Toolchain variant is added to the enum and this function
     *          is not updated, the fallthrough returns "unknown" rather than
     *          terminating -- callers should treat that as an error condition.
     */
    std::string toolchain_name(Toolchain toolchain) {
        switch (toolchain) {
            case Toolchain::None: return "none";
            case Toolchain::Autotools: return "autotools";
            case Toolchain::CMake: return "cmake";
            case Toolchain::Make: return "make";
        }
        return "unknown";
    }

    /**
     * @brief Prints the usage help text for the `kez uconf` subcommand.
     *
     * Outputs a one-line synopsis to stdout describing the expected command-line
     * form: `kez uconf <package>... [--save FILE]`.  This function does not
     * terminate the program -- it simply prints and returns.
     */
    void print_uconf_help() { std::cout << "Usage: kez uconf <package>... [--save FILE]\n"; }

    /**
     * @brief Returns a printable string representation of a Property value.
     *
     * Inspects the variant stored in the Property.  If the Property holds a plain
     * std::string, that string is returned directly.  If it holds a
     * ConfigurableValue<std::string>, the function returns the default value if
     * one is present, or the placeholder "<conditional>" when the value depends
     * on a condition that could not be resolved at generation time.
     *
     * @param property  The Property whose value should be rendered.
     * @return std::string  The resolved value string, or "<conditional>" if the
     *                      ConfigurableValue has no default.
     */
    std::string property_value(const Property& property) {
        if (std::holds_alternative<std::string>(property.data)) {
            return std::get<std::string>(property.data);
        }
        const ConfigurableValue<std::string>& value =
            std::get<ConfigurableValue<std::string>>(property.data);
        return value.default_value.value_or("<conditional>");
    }
}  // namespace

/**
 * @brief Executes the `kez uconf` subcommand: generates a user-configuration
 *        template for one or more packages.
 *
 * Parses the argument list to extract a list of package names and an optional
 * `--save` / `--save=<FILE>` flag.  If `--save` is provided the generated YAML
 * configuration is written to the specified file (interactive mode); otherwise
 * it is dumped to stdout.  The function calls `gen_user_config()` to produce
 * the YAML configuration tree from the parsed package list.
 *
 * **Error handling (all terminate via exit(EXIT_FAILURE)):**
 *   - Missing argument value after `-s` or `--save`.
 *   - Unknown options (any argument starting with `-` that is not `--save`).
 *   - No package names provided.
 *
 * @param arguments  The complete list of arguments passed to the `kez uconf`
 *                   subcommand.  Expected form:
 *                   `[package...] [--save <FILE> | --save=<FILE>]`.
 *                   May be empty or start with `-h`/`--help` to trigger usage.
 */
void execute_uconf(const CommandArguments& arguments) {
    if (arguments.empty() || arguments.front() == "-h" || arguments.front() == "--help") {
        print_uconf_help();
        return;
    }

    std::vector<std::string> packages;
    std::string output_path;
    bool interactive = false;
    for (std::size_t index = 0; index < arguments.size(); ++index) {
        const std::string& argument = arguments[index];
        if (argument == "-s" || argument == "--save") {
            if (++index >= arguments.size()) {
                ERROR("Missing value for " + argument);
                exit(EXIT_FAILURE);
            }
            output_path = arguments[index];
            interactive = true;
        } else if (argument.rfind("--save=", 0) == 0) {
            output_path = argument.substr(7);
            interactive = true;
        } else if (!argument.empty() && argument.front() == '-') {
            ERROR("Unknown template option: " + argument);
            exit(EXIT_FAILURE);
        } else {
            packages.push_back(argument);
        }
    }
    if (packages.empty()) {
        ERROR("At least one package is required");
        exit(EXIT_FAILURE);
    }

    const YAML::Node config = gen_user_config(packages, interactive);
    if (output_path.empty()) {
        std::cout << YAML::Dump(config) << '\n';
    } else {
        write_yaml(config, output_path, "Configuration template written to: " + output_path);
    }
}

/**
 * @brief Executes the `kez info` subcommand: displays metadata for a single
 *        package.
 *
 * Supports two output modes:
 *   - **Normal mode** (default): reads the fully-parsed `PackageConfig` for the
 *     named package via `get_db_config()` and prints its name, description,
 *     author, type, toolchain, releases, implementations, dependencies, and
 *     properties in a human-readable indented format.
 *   - **Raw mode** (`--raw` or `-r`): reads the raw YAML file from the database
 *     directory and dumps its contents verbatim to stdout.
 *
 * **Error handling (all terminate via exit(EXIT_FAILURE)):**
 *   - More than two arguments (i.e. more than one package name + an optional
 *     `--raw` flag).
 *   - A second argument that is not `--raw` or `-r`.
 *   - The raw file read returns an empty string (file missing or unreadable).
 *
 * @param arguments  The argument list for the `kez info` subcommand.  Expected
 *                   forms:
 *                     - empty or `["-h"|"--help"]` prints usage and returns.
 *                     - `["<package>"]` (normal mode).
 *                     - `["<package>", "-r"|"--raw"]` (raw mode).
 */
void execute_info(const CommandArguments& arguments) {
    if (arguments.empty() || arguments.front() == "-h" || arguments.front() == "--help") {
        std::cout << "Usage: kez info <package> [--raw]\n";
        return;
    }
    if (arguments.size() > 2 ||
        (arguments.size() == 2 && arguments[1] != "-r" && arguments[1] != "--raw")) {
        ERROR("Usage: kez info <package> [--raw]");
        exit(EXIT_FAILURE);
    }

    const std::string& name = arguments.front();
    const bool raw          = arguments.size() == 2;
    if (raw) {
        const std::filesystem::path path =
            select_config_path(get_env_var("KEZ_DB"), name, "latest");
        const std::string contents = read_file(path.string());
        if (contents.empty()) {
            ERROR("Failed to read package configuration: " + path.string());
            exit(EXIT_FAILURE);
        }
        std::cout << contents;
        if (contents.back() != '\n') {
            std::cout << '\n';
        }
        return;
    }

    const PackageConfigPtr package = get_db_config(name);
    std::cout << package->name << '\n';
    if (package->description.has_value()) {
        std::cout << "  Description: " << *package->description << '\n';
    }
    if (package->author.has_value()) {
        std::cout << "  Author:      " << *package->author << '\n';
    }
    std::cout << "  Type:        " << package_type_name(package->type) << '\n';
    std::cout << "  Toolchain:   " << toolchain_name(package->toolchain()) << '\n';
    if (package->source.has_value()) {
        std::cout << "  Releases:\n";
        for (const Release& release : package->source->releases) {
            std::cout << "    - " << release.version << '\n';
        }
    }
    if (!package->implementations.empty()) {
        std::cout << "  Implementations:\n";
        for (const std::string& implementation : package->implementations) {
            std::cout << "    - " << implementation << '\n';
        }
    }
    if (!package->dependencies.empty()) {
        std::cout << "  Dependencies:\n";
        for (const std::string& dependency : package->dependencies) {
            std::cout << "    - " << dependency << '\n';
        }
    }
    if (!package->properties.empty()) {
        std::cout << "  Properties:\n";
        for (const Property& property : package->properties) {
            std::cout << "    " << property.name << ": " << property_value(property) << '\n';
        }
    }
}

/**
 * @brief Executes the `kez selfcheck` subcommand: validates the integrity of
 *        the entire package database.
 *
 * Iterates over every subdirectory (package) in the `KEZ_DB` directory and for
 * each one:
 *   - Calls `get_db_config()` to parse and validate the primary `latest.yaml`
 *     recipe (including version-range selection and overlap detection).
 *   - Iterates over every `.yaml` file in the package directory; any file whose
 *     name is not `latest.yaml` is parsed via `parse_db_config()`.
 *
 * All configurations are counted and reported.  If any parse or validation
 * step fails, the called functions will print an error and terminate via
 * `exit(EXIT_FAILURE)`.
 *
 * **Error handling (all terminate via exit(EXIT_FAILURE)):**
 *   - Arguments other than `-h`/`--help` are rejected (selfcheck takes none).
 *   - The `KEZ_DB` environment variable points to a non-existent directory.
 *   - Any individual configuration file fails to parse (delegated to
 *     `get_db_config()` / `parse_db_config()`).
 *
 * @param arguments  The argument list for `kez selfcheck`.  Must be empty
 *                   or `["-h"|"--help"]` (which prints usage and returns).
 */
void execute_selfcheck(const CommandArguments& arguments) {
    if (!arguments.empty()) {
        if (arguments.size() == 1 && (arguments.front() == "-h" || arguments.front() == "--help")) {
            std::cout << "Usage: kez selfcheck\n";
            return;
        }
        ERROR("selfcheck does not accept arguments");
        exit(EXIT_FAILURE);
    }

    const std::filesystem::path database = get_env_var("KEZ_DB");
    if (!std::filesystem::is_directory(database)) {
        ERROR("Database directory does not exist: " + database.string());
        exit(EXIT_FAILURE);
    }
    std::vector<std::string> packages;
    for (const auto& entry : std::filesystem::directory_iterator(database)) {
        if (entry.is_directory()) {
            packages.push_back(entry.path().filename().string());
        }
    }
    std::sort(packages.begin(), packages.end());
    std::size_t configurations = 0;
    for (const std::string& package : packages) {
        get_db_config(package);  // Also validates version-range selection and overlap.
        for (const auto& entry : std::filesystem::directory_iterator(database / package)) {
            if (entry.is_regular_file() && entry.path().extension() == ".yaml") {
                if (entry.path().filename() != "latest.yaml") {
                    parse_db_config(entry.path());
                }
                ++configurations;
            }
        }
    }
    SUCCESS("Validated " + std::to_string(configurations) + " configurations for " +
            std::to_string(packages.size()) + " packages.");
}
