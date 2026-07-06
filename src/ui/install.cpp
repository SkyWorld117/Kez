#include <unistd.h>
#include <yaml-cpp/yaml.h>

#include <cmdline_parser/cmdline_parser.hpp>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <ui/commands.hpp>
#include <ui/ui_utils.hpp>
#include <user_config_generator/user_config_generator.hpp>
#include <utils/bash_utils.hpp>
#include <utils/colored_io.hpp>
#include <vector>

namespace {
    struct InstallOptions {
        bool read_file  = false;
        bool dry_run    = false;
        bool force      = false;
        bool with_slurm = false;
        std::string environment;
        std::vector<std::string> overrides;
        std::vector<std::string> positional;
    };

    void print_install_help(bool utility) {
        if (utility) {
            std::cout
                << "Usage: kez utilities add [options] <package>...\n\n"
                   "Options:\n"
                   "  -r, --read             Treat the positional argument as a YAML file\n"
                   "  -d, --dry-run          Show the commands that would be executed\n"
                   "  -c, --config PATH=VAL  Override a generated configuration value\n"
                   "  -f, --force            Reinstall packages already recorded in state.yaml\n"
                   "  -S, --with-slurm       Run scripts/install.sh through sbatch\n";
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
                   "  -S, --with-slurm       Run scripts/install.sh through sbatch\n";
        }
    }

    std::string required_value(const CommandArguments& arguments, std::size_t& index,
                               const std::string& option) {
        if (index + 1 >= arguments.size()) {
            ERROR("Missing value for " + option);
            exit(EXIT_FAILURE);
        }
        return arguments[++index];
    }

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

    void install(const CommandArguments& arguments, bool utility) {
        bool help                    = false;
        const InstallOptions options = parse_install_options(arguments, utility, help);
        if (help) {
            print_install_help(utility);
            return;
        }

        YAML::Node user_config = load_install_config(options);
        apply_cmdline_config(user_config, options.overrides);
        const std::filesystem::path prefix =
            installation_prefix(user_config, options.environment, utility);

        std::error_code error;
        std::filesystem::create_directories(prefix / ".tmp", error);
        if (error) {
            ERROR("Failed to create installation environment: " + error.message());
            exit(EXIT_FAILURE);
        }

        const BashCommandPlan plan = parse_cmdline(user_config, prefix);
        if (options.dry_run) {
            print_command_plan(plan);
            return;
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

        std::string command = "bash " + shell_single_quote(script.string()) + " " +
                              shell_single_quote(prefix.string()) + " " +
                              shell_single_quote(plan_path.string());
        if (options.force) {
            command += " --force";
        }
        if (options.with_slurm) {
            command = "sbatch --wait --job-name=kez-install --wrap=" + shell_single_quote(command);
        }
        run_external_command(command);
        std::filesystem::remove(plan_path, error);
        if (error) {
            WARNING("Could not remove installation plan: " + error.message());
        }
    }

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

void execute_install(const CommandArguments& arguments) { install(arguments, false); }

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
