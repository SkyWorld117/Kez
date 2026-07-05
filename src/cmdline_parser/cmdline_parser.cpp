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

void apply_cmdline_config(YAML::Node user_config, const std::vector<std::string>& config_options) {
    if (!yaml_has(user_config, "cheese") || !user_config["cheese"].IsMap()) {
        ERROR("Invalid user configuration: cheese must be a map");
        exit(EXIT_FAILURE);
    }

    for (const std::string& option : config_options) {
        const std::size_t separator = option.find('=');
        if (separator == std::string::npos || separator == 0) {
            ERROR("Invalid configuration override '" + option + "'; expected <path>=<value>");
            exit(EXIT_FAILURE);
        }
        traverse(option.substr(0, separator), option.substr(separator + 1), user_config["cheese"]);
    }
}

BashCommandPlan parse_cmdline(const std::filesystem::path& file,
                              const std::filesystem::path& install_prefix,
                              const std::vector<std::string>& config_options) {
    if (!std::filesystem::is_regular_file(file)) {
        ERROR("User configuration file does not exist: " + file.string());
        exit(EXIT_FAILURE);
    }
    return parse_cmdline(YAML::LoadFile(file.string()), install_prefix, config_options);
}

BashCommandPlan parse_cmdline(const std::vector<std::string>& targets,
                              const std::filesystem::path& install_prefix,
                              const std::vector<std::string>& config_options) {
    if (targets.empty()) {
        ERROR("At least one target package is required");
        exit(EXIT_FAILURE);
    }
    return parse_cmdline(gen_user_config(targets, false), install_prefix, config_options);
}

BashCommandPlan parse_cmdline(YAML::Node user_config, const std::filesystem::path& install_prefix,
                              const std::vector<std::string>& config_options) {
    apply_cmdline_config(user_config, config_options);
    return parse_user_config(user_config, "release", install_prefix);
}

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
