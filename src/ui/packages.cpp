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
     * @brief Prints the usage message for the `uconf` subcommand.
     */
    void print_uconf_help() { std::cout << "Usage: kez uconf <package>... [--save FILE]\n"; }

    /**
     * @brief Extracts the display value from a Property, preferring the default.
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
 * @brief Runs the `uconf` subcommand: generates a user configurable YAML for the
 *        given packages, optionally writing it to a file with --save.
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
            ERROR("Unknown uconf option: " + argument);
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
        write_yaml(config, output_path, "Configurable YAML written to: " + output_path);
    }
}

/**
 * @brief Runs the `info` subcommand: displays metadata for a single package or prints
 *        its raw recipe YAML with --raw.
 *
 * The formatted report uses a layout similar to `fgr info`:
 * - A decorated title block (name, description, author, type, toolchain)
 * - Section-based output with per-type variation:
 *   - Abstract packages show their implementations.
 *   - External packages show their properties.
 *   - All other types show releases, config options, dependencies, and properties.
 * - Colored delimiters, consistent column alignment via print_two_columns, and
 *   word-wrapping through print_text.
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

    // ---- layout constants matching fgr info style ----
    const int max_width   = 80;
    const int short_width = 30;
    const int indent      = 4;

    auto strong_color = [](const std::string& txt) {
        return color<Color::BOLD, Color::MAGENTA>(txt);
    };
    auto normal_color = [](const std::string& txt) { return color<Color::MAGENTA>(txt); };

    auto print_section_header = [&](const std::string& title) {
        const std::string delim(short_width, '-');
        print_text("");
        print_text(strong_color(delim));
        print_text(strong_color(title + ":"));
        print_text("");
    };

    // ---- title block ----
    {
        const std::string large_delim(max_width, '=');
        print_text(strong_color(large_delim));
        print_text(strong_color(package->name), max_width);
        print_text(strong_color(large_delim));

        if (package->description) {
            print_text(*package->description, max_width);
        }
        print_text("");
        print_text(normal_color("Author: ") + (package->author ? *package->author : "{missing}"),
                   max_width);
        print_text("");

        print_text(normal_color("Type: ") + package_type_name(package->type), max_width);

        const std::string tc = toolchain_name(package->toolchain());
        if (tc != "none" && tc != "unknown") {
            print_text(normal_color("Toolchain: ") + tc, max_width);
        }
    }

    // ---- type-specific body sections ----
    if (package->type == PackageType::Abstract) {
        // --- Implements ---
        print_section_header("Implements");
        if (!package->implementations.empty()) {
            for (const std::string& impl : package->implementations) {
                print_two_columns(impl, "{Short description}", short_width, max_width);
            }
        } else {
            print_text("Nothing");
        }

    } else if (package->type == PackageType::External) {
        // --- Properties ---
        print_section_header("Properties");
        if (!package->properties.empty()) {
            for (const Property& prop : package->properties) {
                print_two_columns(prop.name, property_value(prop), short_width, 0);
            }
        } else {
            print_text("None");
        }

    } else {
        // --- Releases ---
        print_section_header("Releases");
        if (package->source) {
            // Map SourceType to string.
            auto source_type_str = [](SourceType st) -> std::string {
                switch (st) {
                    case SourceType::Git: return "git";
                    case SourceType::Tarball: return "tarball";
                    case SourceType::Zip: return "zip";
                    case SourceType::Script: return "script";
                }
                return "unknown";
            };
            print_text("Type: " + normal_color(source_type_str(package->source->type)), max_width);

            if (package->source->type == SourceType::Git && package->source->url) {
                print_text("Repository: " + normal_color(*package->source->url));
            }

            for (const Release& release : package->source->releases) {
                print_text("Version " + normal_color(release.version), max_width);
                if (package->source->type == SourceType::Git && release.tag) {
                    print_text(*release.tag, 0, indent);
                } else if (release.url) {
                    print_text(*release.url, 0, indent);
                }
            }
        } else {
            print_text("None");
        }

        // --- Config Options ---
        print_section_header("Config Options");
        {
            bool found = false;

            // Collect from the top-level build configuration.
            if (package->build && package->build->configurations) {
                const BuildConfiguration& cfg = *package->build->configurations;

                for (const BuildOption& opt : cfg.options) {
                    if (!opt.user_configurable) {
                        continue;
                    }
                    found             = true;
                    std::string label = normal_color(opt.name);
                    if (opt.enabled_value && opt.enabled_value->default_value) {
                        label += " [" + *opt.enabled_value->default_value + "]";
                    }
                    print_two_columns(label, opt.description.value_or(""), short_width, max_width);
                }

                for (const EnvironmentVariable& env : cfg.environment) {
                    if (!env.user_configurable) {
                        continue;
                    }
                    found = true;
                    print_two_columns(normal_color(env.name), env.description.value_or(""),
                                      short_width, max_width);
                }
            }

            // Collect from per-stage configurations.
            if (package->build) {
                for (const BuildStage& stage : package->build->stages) {
                    if (!stage.configurations) {
                        continue;
                    }
                    for (const BuildOption& opt : stage.configurations->options) {
                        if (!opt.user_configurable) {
                            continue;
                        }
                        found             = true;
                        std::string label = normal_color(opt.name);
                        if (opt.enabled_value && opt.enabled_value->default_value) {
                            label += " [" + *opt.enabled_value->default_value + "]";
                        }
                        print_two_columns(label, opt.description.value_or(""), short_width,
                                          max_width);
                    }
                    for (const EnvironmentVariable& env : stage.configurations->environment) {
                        if (!env.user_configurable) {
                            continue;
                        }
                        found = true;
                        print_two_columns(normal_color(env.name), env.description.value_or(""),
                                          short_width, max_width);
                    }
                }
            }

            if (!found) {
                print_text("None");
            }
        }

        // --- Dependencies ---
        print_section_header("Dependencies");
        if (!package->dependencies.empty()) {
            for (const std::string& dep : package->dependencies) {
                print_two_columns(dep, "{Short description}", short_width, max_width);
            }
        } else {
            print_text("None");
        }

        // --- Properties ---
        print_section_header("Properties");
        if (!package->properties.empty()) {
            for (const Property& prop : package->properties) {
                print_two_columns(prop.name, property_value(prop), short_width, 0);
            }
        } else {
            print_text("None");
        }
    }

    print_text("");
}

/**
 * @brief Runs the `selfcheck` subcommand: validates every recipe in the database
 *        directory, reporting the total count of validated configurations.
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
