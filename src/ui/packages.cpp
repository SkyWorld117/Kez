#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cstdlib>
#include <database/config.hpp>
#include <database/config_selector.hpp>
#include <database/database.hpp>
#include <filesystem>
#include <iostream>
#include <string>
#include <uconf_generator/uconf_generator.hpp>
#include <uconf_parser/user_config_parser.hpp>
#include <ui/argparse.hpp>
#include <ui/commands.hpp>
#include <ui/packages.hpp>
#include <ui/ui_utils.hpp>
#include <utils/bash_utils.hpp>
#include <utils/colored_io.hpp>
#include <utils/file_utils.hpp>
#include <variant>
#include <vector>

namespace {

    void append_configuration_entries(const BuildConfiguration& configuration,
                                      std::vector<PackageConfigEntry>& result) {
        for (const BuildOption& option : configuration.options) {
            if (option.user_configurable) {
                result.push_back({option.name,
                                  option.enabled_value.has_value()
                                      ? option.enabled_value->default_value
                                      : std::nullopt,
                                  option.description.value_or("")});
            }
        }
        for (const EnvironmentVariable& environment : configuration.environment) {
            if (environment.user_configurable) {
                result.push_back(
                    {environment.name, std::nullopt, environment.description.value_or("")});
            }
        }
    }

}  // namespace

std::vector<PackageConfigEntry> package_config_entries(const PackageConfig& package) {
    std::vector<PackageConfigEntry> result;
    if (!package.build.has_value()) {
        return result;
    }
    if (package.build->configurations.has_value()) {
        append_configuration_entries(*package.build->configurations, result);
    }
    for (const BuildStage& stage : package.build->stages) {
        if (stage.configurations.has_value()) {
            append_configuration_entries(*stage.configurations, result);
        }
    }
    return result;
}

std::string toolchain_name(Toolchain toolchain) {
    switch (toolchain) {
        case Toolchain::None: return "none";
        case Toolchain::Autotools: return "autotools";
        case Toolchain::CMake: return "cmake";
        case Toolchain::Make: return "make";
        case Toolchain::Meson: return "meson";
        case Toolchain::Python: return "python";
    }
    return "unknown";
}

std::string source_type_name(SourceType source_type) {
    switch (source_type) {
        case SourceType::Git: return "git";
        case SourceType::Tarball: return "tarball";
        case SourceType::Zip: return "zip";
        case SourceType::Script: return "script";
        case SourceType::PyPI: return "pypi";
    }
    return "unknown";
}

std::string property_display_value(const Property& property) {
    if (std::holds_alternative<std::string>(property.data)) {
        return std::get<std::string>(property.data);
    }
    const ConfigurableValue<std::string>& value =
        std::get<ConfigurableValue<std::string>>(property.data);
    return value.default_value.value_or("<conditional>");
}

namespace {
    /**
     * @brief Prints the usage message for the `uconf` subcommand.
     */
    void print_uconf_help() { INFO("Usage: kez uconf <package>... [--save FILE] [--silence]"); }

}  // namespace

/**
 * @brief Runs the `uconf` subcommand: generates a user configurable YAML for the
 *        given packages, optionally writing it to a file with --save.
 */
void execute_uconf(const CommandArguments& arguments) {
    const UconfOptionsParseResult parsed = parse_uconf_options(arguments);
    if (!parsed.error.empty()) {
        ERROR(parsed.error);
        exit(EXIT_FAILURE);
    }
    if (parsed.help) {
        print_uconf_help();
        return;
    }

    const YAML::Node config = gen_user_config(parsed.options.packages, !parsed.options.silent);
    if (parsed.options.output_path.empty()) {
        std::cout << YAML::Dump(config) << '\n';
    } else {
        write_yaml(config, parsed.options.output_path,
                   "Configurable YAML written to: " + parsed.options.output_path);
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
    const InfoArgumentsParseResult parsed = parse_info_arguments(arguments);
    if (!parsed.error.empty()) {
        ERROR(parsed.error);
        exit(EXIT_FAILURE);
    }
    if (parsed.help) {
        INFO("Usage: kez info <package> [--raw]");
        return;
    }

    const std::string& name = parsed.arguments.package;
    if (parsed.arguments.raw) {
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
                print_two_columns(prop.name, property_display_value(prop), short_width, 0);
            }
        } else {
            print_text("None");
        }

    } else {
        // --- Releases ---
        print_section_header("Releases");
        if (package->source) {
            print_text("Type: " + normal_color(source_type_name(package->source->type)), max_width);

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
        const std::vector<PackageConfigEntry> entries = package_config_entries(*package);
        if (entries.empty()) {
            print_text("None");
        } else {
            for (const PackageConfigEntry& entry : entries) {
                std::string label = normal_color(entry.name);
                if (entry.default_value.has_value()) {
                    label += " [" + *entry.default_value + "]";
                }
                print_two_columns(label, entry.description, short_width, max_width);
            }
        }

        // --- Dependencies ---
        print_section_header("Dependencies");
        if (!package->dependencies.empty()) {
            for (const Dependency& dep : package->dependencies) {
                std::string label = dep.name;
                if (!dep.constraints.empty()) {
                    label += " (";
                    for (std::size_t i = 0; i < dep.constraints.size(); ++i) {
                        if (i > 0) label += ", ";
                        label += dep.constraints[i].op + " " + dep.constraints[i].version;
                    }
                    label += ")";
                }
                print_two_columns(label, "{Short description}", short_width, max_width);
            }
        } else {
            print_text("None");
        }

        // --- Properties ---
        print_section_header("Properties");
        if (!package->properties.empty()) {
            for (const Property& prop : package->properties) {
                print_two_columns(prop.name, property_display_value(prop), short_width, 0);
            }
        } else {
            print_text("None");
        }
    }

    print_text("");
}

/**
 * @brief Runs the `dbcheck` subcommand for all recipes or an explicit package selection.
 */
void execute_dbcheck(const CommandArguments& arguments) {
    const DbCheckOptionsParseResult parsed = parse_dbcheck_options(arguments);
    if (!parsed.error.empty()) {
        ERROR(parsed.error);
        exit(EXIT_FAILURE);
    }
    if (parsed.help) {
        INFO("Usage: kez dbcheck [--only <package>...]");
        return;
    }

    const std::filesystem::path database = get_env_var("KEZ_DB");
    if (!std::filesystem::is_directory(database)) {
        ERROR("Database directory does not exist: " + database.string());
        exit(EXIT_FAILURE);
    }

    std::vector<std::string> packages = parsed.packages;
    if (parsed.all_packages) {
        for (const auto& entry : std::filesystem::directory_iterator(database)) {
            if (entry.is_directory()) {
                packages.push_back(entry.path().filename().string());
            }
        }
        std::sort(packages.begin(), packages.end());
    }

    std::size_t configurations = 0;
    for (const std::string& package : packages) {
        const std::filesystem::path package_directory = database / package;
        if (!std::filesystem::is_directory(package_directory)) {
            ERROR("Package not found in database: " + package);
            exit(EXIT_FAILURE);
        }
        get_db_config(package);
        for (const auto& entry : std::filesystem::directory_iterator(package_directory)) {
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
