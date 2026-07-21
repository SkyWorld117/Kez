/**
 * @file argparse.cpp
 * @brief Non-terminating argument parsing for all UI commands.
 */

#include <string>
#include <ui/argparse.hpp>
#include <utility>

namespace {

    bool consume_required_value(const CommandArguments& arguments, std::size_t& index,
                                const std::string& option, std::string& value, std::string& error) {
        if (index + 1 >= arguments.size()) {
            error = "Missing value for " + option;
            return false;
        }
        value = arguments[++index];
        return true;
    }

    bool parse_exact_name(const CommandArguments& arguments, const std::string& description,
                          std::string& name, std::string& error) {
        if (arguments.size() != 2) {
            error = description + " requires exactly one name";
            return false;
        }
        name = arguments[1];
        return true;
    }

    bool parse_no_arguments(const CommandArguments& arguments, const std::string& description,
                            std::string& error) {
        if (arguments.size() != 1) {
            error = description + " does not accept additional arguments";
            return false;
        }
        return true;
    }

}  // namespace

UiArgumentsParseResult parse_ui_arguments(const CommandArguments& arguments) {
    UiArgumentsParseResult result;
    if (arguments.empty() || arguments.front() == "-h" || arguments.front() == "--help") {
        return result;
    }

    const std::string& command = arguments.front();
    if (command == "-V" || command == "--version") {
        result.command = UiCommand::Version;
        return result;
    }
    if (command == "init") {
        result.command = UiCommand::Init;
    } else if (command == "update") {
        result.command = UiCommand::Update;
    } else if (command == "install") {
        result.command = UiCommand::Install;
    } else if (command == "utilities") {
        result.command = UiCommand::Utilities;
    } else if (command == "uconf") {
        result.command = UiCommand::Uconf;
    } else if (command == "env") {
        result.command = UiCommand::Environment;
    } else if (command == "compiler") {
        result.command = UiCommand::Compiler;
    } else if (command == "mpi") {
        result.command = UiCommand::Mpi;
    } else if (command == "factory") {
        result.command = UiCommand::Factory;
    } else if (command == "info") {
        result.command = UiCommand::Info;
    } else if (command == "dbcheck") {
        result.command = UiCommand::DbCheck;
    } else {
        result.error = "Unknown command: " + command;
        return result;
    }
    result.arguments.assign(arguments.begin() + 1, arguments.end());
    return result;
}

InitOptionsParseResult parse_init_options(const CommandArguments& arguments) {
    InitOptionsParseResult result;
    for (const std::string& argument : arguments) {
        if (argument == "-h" || argument == "--help") {
            result.help = true;
            return result;
        }
        if (argument == "--force") {
            result.options.force = true;
        } else if (argument == "--use-distro-compiler") {
            result.options.use_distro_compiler = true;
        } else {
            result.error = "Unknown init option: " + argument;
            return result;
        }
    }
    return result;
}

UpdateOptionsParseResult parse_update_options(const CommandArguments& arguments) {
    UpdateOptionsParseResult result;
    for (const std::string& argument : arguments) {
        if (argument == "-h" || argument == "--help") {
            result.help = true;
            return result;
        }
        if (argument == "--with-system") {
            result.options.with_system = true;
        } else {
            result.error = "Unknown update option: " + argument;
            return result;
        }
    }
    return result;
}

InstallOptionsParseResult parse_install_options(const CommandArguments& arguments, bool utility) {
    InstallOptionsParseResult result;
    bool positional_only = false;
    for (std::size_t index = 0; index < arguments.size(); ++index) {
        const std::string& argument = arguments[index];
        if (positional_only) {
            result.options.positional.push_back(argument);
        } else if (argument == "--") {
            positional_only = true;
        } else if (argument == "-h" || argument == "--help") {
            result.help = true;
        } else if (argument == "-r" || argument == "--read") {
            result.options.read_file = true;
        } else if (argument.rfind("--read=", 0) == 0) {
            result.options.read_file = true;
            result.options.positional.push_back(argument.substr(7));
        } else if (argument == "-d" || argument == "--dry-run") {
            result.options.dry_run = true;
        } else if (argument == "-f" || argument == "--force") {
            result.options.force = true;
        } else if (argument == "-S" || argument == "--with-slurm") {
            result.options.with_slurm = true;
        } else if (argument == "--silence") {
            result.options.silent = true;
        } else if (argument == "--rename") {
            if (utility) {
                result.error = "--rename is not valid for utility installation";
                return result;
            }
            if (!consume_required_value(arguments, index, argument, result.options.renamed_version,
                                        result.error)) {
                return result;
            }
            if (result.options.renamed_version.empty()) {
                result.error = "Missing value for --rename";
                return result;
            }
        } else if (argument.rfind("--rename=", 0) == 0) {
            if (utility) {
                result.error = "--rename is not valid for utility installation";
                return result;
            }
            result.options.renamed_version = argument.substr(9);
            if (result.options.renamed_version.empty()) {
                result.error = "Missing value for --rename";
                return result;
            }
        } else if (argument == "-R" || argument == "--rebuild") {
            if (utility) {
                result.error = "--rebuild is not valid for utility installation";
                return result;
            }
            result.options.rebuild = true;
            if (!consume_required_value(arguments, index, argument, result.options.rebuild_package,
                                        result.error)) {
                return result;
            }
        } else if (argument.rfind("--rebuild=", 0) == 0) {
            if (utility) {
                result.error = "--rebuild is not valid for utility installation";
                return result;
            }
            result.options.rebuild         = true;
            result.options.rebuild_package = argument.substr(10);
        } else if (argument == "-e" || argument == "--env") {
            if (utility) {
                result.error = argument + " is not valid for utility installation";
                return result;
            }
            if (!consume_required_value(arguments, index, argument, result.options.environment,
                                        result.error)) {
                return result;
            }
        } else if (argument.rfind("--env=", 0) == 0) {
            if (utility) {
                result.error = "--env is not valid for utility installation";
                return result;
            }
            result.options.environment = argument.substr(6);
        } else if (argument == "-c" || argument == "--config") {
            std::string override_value;
            if (!consume_required_value(arguments, index, argument, override_value, result.error)) {
                return result;
            }
            result.options.overrides.push_back(std::move(override_value));
            while (index + 1 < arguments.size() && !arguments[index + 1].empty() &&
                   arguments[index + 1].find('=') != std::string::npos &&
                   arguments[index + 1].front() != '-') {
                result.options.overrides.push_back(arguments[++index]);
            }
        } else if (argument.rfind("--config=", 0) == 0) {
            result.options.overrides.push_back(argument.substr(9));
        } else if (!argument.empty() && argument.front() == '-') {
            result.error = "Unknown install option: " + argument;
            return result;
        } else {
            result.options.positional.push_back(argument);
        }
    }
    return result;
}

UtilitiesArgumentsParseResult parse_utilities_arguments(const CommandArguments& arguments) {
    UtilitiesArgumentsParseResult result;
    if (arguments.empty() || arguments.front() == "-h" || arguments.front() == "--help") {
        return result;
    }
    const std::string& action = arguments.front();
    if (action == "add") {
        result.arguments.action = UtilitiesAction::Add;
        result.arguments.install_arguments.assign(arguments.begin() + 1, arguments.end());
    } else if (action == "remove") {
        result.arguments.action = UtilitiesAction::Remove;
        if (arguments.size() != 2) {
            result.error = "utilities remove requires exactly one package name";
        } else {
            result.arguments.package = arguments[1];
        }
    } else if (action == "empty") {
        result.arguments.action = UtilitiesAction::Empty;
        if (arguments.size() != 1) {
            result.error = "utilities empty does not accept additional arguments";
        }
    } else {
        result.error = "Unknown utilities command: " + action;
    }
    return result;
}

UconfOptionsParseResult parse_uconf_options(const CommandArguments& arguments) {
    UconfOptionsParseResult result;
    if (arguments.empty() || arguments.front() == "-h" || arguments.front() == "--help") {
        result.help = true;
        return result;
    }
    for (std::size_t index = 0; index < arguments.size(); ++index) {
        const std::string& argument = arguments[index];
        if (argument == "-s" || argument == "--save") {
            if (++index >= arguments.size()) {
                result.error = "Missing value for " + argument;
                return result;
            }
            result.options.output_path = arguments[index];
        } else if (argument.rfind("--save=", 0) == 0) {
            result.options.output_path = argument.substr(7);
        } else if (argument == "--silence") {
            result.options.silent = true;
        } else if (!argument.empty() && argument.front() == '-') {
            result.error = "Unknown uconf option: " + argument;
            return result;
        } else {
            result.options.packages.push_back(argument);
        }
    }
    if (result.options.packages.empty()) {
        result.error = "At least one package is required";
    }
    return result;
}

InfoArgumentsParseResult parse_info_arguments(const CommandArguments& arguments) {
    InfoArgumentsParseResult result;
    if (arguments.empty() || arguments.front() == "-h" || arguments.front() == "--help") {
        result.help = true;
        return result;
    }
    if (arguments.size() > 2 ||
        (arguments.size() == 2 && arguments[1] != "-r" && arguments[1] != "--raw")) {
        result.error = "Usage: kez info <package> [--raw]";
        return result;
    }
    result.arguments.package = arguments.front();
    result.arguments.raw     = arguments.size() == 2;
    return result;
}

DbCheckOptionsParseResult parse_dbcheck_options(const CommandArguments& arguments) {
    DbCheckOptionsParseResult result;
    if (arguments.empty()) {
        result.all_packages = true;
        return result;
    }
    if (arguments.size() == 1 && (arguments.front() == "-h" || arguments.front() == "--help")) {
        result.help = true;
        return result;
    }
    if (arguments.front() != "--only") {
        result.error = "Unknown dbcheck option: " + arguments.front();
        return result;
    }
    for (std::size_t index = 1; index < arguments.size(); ++index) {
        const std::string& argument = arguments[index];
        if (!argument.empty() && argument.front() == '-') {
            result.error = "Unknown dbcheck option: " + argument;
            return result;
        }
        result.packages.push_back(argument);
    }
    if (result.packages.empty()) {
        result.error = "--only requires at least one package name";
    }
    return result;
}

EnvironmentArgumentsParseResult parse_environment_arguments(const CommandArguments& arguments) {
    EnvironmentArgumentsParseResult result;
    if (arguments.empty() || arguments.front() == "-h" || arguments.front() == "--help") {
        return result;
    }

    const std::string& action = arguments.front();
    if (action == "list") {
        result.arguments.action = EnvironmentAction::List;
        parse_no_arguments(arguments, "env list", result.error);
    } else if (action == "which") {
        result.arguments.action = EnvironmentAction::Which;
        parse_no_arguments(arguments, "env which", result.error);
    } else if (action == "deactivate") {
        result.arguments.action = EnvironmentAction::Deactivate;
        parse_no_arguments(arguments, "env deactivate", result.error);
    } else if (action == "create") {
        result.arguments.action = EnvironmentAction::Create;
        parse_exact_name(arguments, "env create", result.arguments.name, result.error);
    } else if (action == "remove") {
        result.arguments.action = EnvironmentAction::Remove;
        parse_exact_name(arguments, "env remove", result.arguments.name, result.error);
    } else if (action == "empty") {
        result.arguments.action = EnvironmentAction::Empty;
        parse_exact_name(arguments, "env empty", result.arguments.name, result.error);
    } else if (action == "activate") {
        result.arguments.action = EnvironmentAction::Activate;
        parse_exact_name(arguments, "env activate", result.arguments.name, result.error);
    } else {
        result.error = "Unknown env command: " + action;
    }
    return result;
}

ManagedEnvironmentArgumentsParseResult parse_managed_environment_arguments(
    const CommandArguments& arguments, const std::string& command) {
    ManagedEnvironmentArgumentsParseResult result;
    if (arguments.empty() || arguments.front() == "-h" || arguments.front() == "--help") {
        return result;
    }

    const std::string& action = arguments.front();
    if (action == "list") {
        result.arguments.action = ManagedEnvironmentAction::List;
        parse_no_arguments(arguments, command + " list", result.error);
    } else if (action == "which") {
        result.arguments.action = ManagedEnvironmentAction::Which;
        parse_no_arguments(arguments, command + " which", result.error);
    } else if (action == "unload") {
        result.arguments.action = ManagedEnvironmentAction::Unload;
        parse_no_arguments(arguments, command + " unload", result.error);
    } else if (action == "load") {
        result.arguments.action = ManagedEnvironmentAction::Load;
        parse_exact_name(arguments, command + " load", result.arguments.name, result.error);
    } else if (action == "remove") {
        result.arguments.action = ManagedEnvironmentAction::Remove;
        parse_exact_name(arguments, command + " remove", result.arguments.name, result.error);
    } else {
        result.error = "Unknown " + command + " command: " + action;
    }
    return result;
}

FactoryArgumentsParseResult parse_factory_arguments(const CommandArguments& arguments) {
    FactoryArgumentsParseResult result;
    if (arguments.empty() || arguments.front() == "-h" || arguments.front() == "--help") {
        return result;
    }

    const std::string& action = arguments.front();
    if (action == "create") {
        result.arguments.action = FactoryAction::Create;
        parse_exact_name(arguments, "factory create", result.arguments.name, result.error);
    } else if (action == "remove") {
        result.arguments.action = FactoryAction::Remove;
        parse_exact_name(arguments, "factory remove", result.arguments.name, result.error);
    } else if (action == "enter") {
        result.arguments.action = FactoryAction::Enter;
        parse_exact_name(arguments, "factory enter", result.arguments.name, result.error);
    } else if (action == "list") {
        result.arguments.action = FactoryAction::List;
        parse_no_arguments(arguments, "factory list", result.error);
    } else if (action == "exit") {
        result.arguments.action = FactoryAction::Exit;
        parse_no_arguments(arguments, "factory exit", result.error);
    } else if (action == "which") {
        result.arguments.action = FactoryAction::Which;
        parse_no_arguments(arguments, "factory which", result.error);
    } else if (action == "run") {
        result.arguments.action = FactoryAction::Run;
        parse_no_arguments(arguments, "factory run", result.error);
    } else if (action == "summarize") {
        result.arguments.action = FactoryAction::Summarize;
        parse_no_arguments(arguments, "factory summarize", result.error);
    } else if (action == "build") {
        result.arguments.action = FactoryAction::Build;
        for (std::size_t index = 1; index < arguments.size(); ++index) {
            const std::string& argument = arguments[index];
            if (argument == "-h" || argument == "--help") {
                result.arguments.action = FactoryAction::Help;
                return result;
            }
            if (argument == "-d" || argument == "--dry-run") {
                result.arguments.build_options.dry_run = true;
            } else if (argument == "-f" || argument == "--force") {
                result.arguments.build_options.force = true;
            } else if (argument == "-S" || argument == "--with-slurm") {
                result.arguments.build_options.with_slurm = true;
            } else {
                result.error = "Unknown factory build option: " + argument;
                return result;
            }
        }
    } else {
        result.error = "Unknown factory command: " + action;
    }
    return result;
}
