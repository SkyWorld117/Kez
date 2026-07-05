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
    std::string package_type_name(PackageType type) {
        switch (type) {
            case PackageType::Package: return "package";
            case PackageType::System: return "system";
            case PackageType::Compiler: return "compiler";
            case PackageType::Mpi: return "mpi";
            case PackageType::Vendor: return "vendor";
            case PackageType::Abstract: return "abstract";
            case PackageType::External: return "external";
        }
        return "unknown";
    }

    std::string toolchain_name(Toolchain toolchain) {
        switch (toolchain) {
            case Toolchain::None: return "none";
            case Toolchain::Autotools: return "autotools";
            case Toolchain::CMake: return "cmake";
            case Toolchain::Make: return "make";
        }
        return "unknown";
    }

    void print_template_help() { std::cout << "Usage: kez template <package>... [--save FILE]\n"; }

    std::string property_value(const Property& property) {
        if (std::holds_alternative<std::string>(property.data)) {
            return std::get<std::string>(property.data);
        }
        const ConfigurableValue<std::string>& value =
            std::get<ConfigurableValue<std::string>>(property.data);
        return value.default_value.value_or("<conditional>");
    }
}  // namespace

void execute_template(const CommandArguments& arguments) {
    if (arguments.empty() || arguments.front() == "-h" || arguments.front() == "--help") {
        print_template_help();
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
                parse_db_config(entry.path());
                ++configurations;
            }
        }
    }
    SUCCESS("Validated " + std::to_string(configurations) + " configurations for " +
            std::to_string(packages.size()) + " packages.");
}
