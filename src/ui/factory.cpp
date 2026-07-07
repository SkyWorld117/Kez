#include <unistd.h>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cmdline_parser/cmdline_parser.hpp>
#include <factory/factory.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <system_error>
#include <ui/commands.hpp>
#include <ui/ui_utils.hpp>
#include <utils/bash_utils.hpp>
#include <utils/colored_io.hpp>
#include <utils/file_utils.hpp>
#include <utils/string_utils.hpp>
#include <vector>

namespace {
    struct FactoryBuildOptions {
        bool dry_run    = false;
        bool force      = false;
        bool with_slurm = false;
    };

    void factory_help() {
        std::cout << "Usage: kez factory <create|remove|list|enter|exit|which|build|run|summarize> "
                     "[options]\n\n"
                     "Commands:\n"
                     "  create NAME        Create a factory\n"
                     "  remove NAME        Remove a factory\n"
                     "  list               List factories\n"
                     "  enter NAME         Select a factory in the current shell\n"
                     "  exit               Clear the selected factory\n"
                     "  which              Show the selected factory\n"
                     "  build              Build all recipe YAML files into factory buildspace\n"
                     "  run                Run runspace profiles\n"
                     "  summarize          Print lines matching profile summary regexes\n\n"
                     "Build options:\n"
                     "  -d, --dry-run      Show installation commands without executing them\n"
                     "  -f, --force        Reinstall packages already recorded in state.yaml\n"
                     "  -S, --with-slurm   Run scripts/install.sh through sbatch\n";
    }

    std::string required_factory_name(const CommandArguments& arguments,
                                      const std::string& action) {
        if (arguments.size() != 2) {
            ERROR("factory " + action + " requires exactly one name");
            exit(EXIT_FAILURE);
        }
        validate_path_component(arguments[1], "factory name");
        return arguments[1];
    }

    std::filesystem::path factories_root() { return configured_work_path("factories"); }

    std::filesystem::path factory_path(const std::string& name) {
        validate_path_component(name, "factory name");
        return factories_root() / name;
    }

    std::filesystem::path active_factory_path() {
        const std::string name = get_env_var(
            "KEZ_FACTORY", "No factory is currently selected. Run 'kez factory enter <name>'.");
        return factory_path(name);
    }

    void create_factory(const std::string& name) {
        const std::filesystem::path path = factory_path(name);
        if (std::filesystem::exists(path)) {
            ERROR("Factory already exists: " + name);
            exit(EXIT_FAILURE);
        }
        std::error_code error;
        std::filesystem::create_directories(path / "recipes", error);
        if (!error) {
            std::filesystem::create_directories(path / "buildspace", error);
        }
        if (!error) {
            std::filesystem::create_directories(path / "runspace", error);
        }
        if (error) {
            ERROR("Failed to create factory: " + error.message());
            exit(EXIT_FAILURE);
        }
        SUCCESS("Factory created: " + name);
    }

    void remove_factory(const std::string& name) {
        const std::filesystem::path path = factory_path(name);
        if (!std::filesystem::is_directory(path)) {
            ERROR("Factory does not exist: " + name);
            exit(EXIT_FAILURE);
        }
        std::error_code error;
        std::filesystem::remove_all(path, error);
        if (error) {
            ERROR("Failed to remove factory: " + error.message());
            exit(EXIT_FAILURE);
        }
        SUCCESS("Factory removed: " + name);
    }

    FactoryBuildOptions parse_build_options(const CommandArguments& arguments) {
        FactoryBuildOptions result;
        for (std::size_t index = 1; index < arguments.size(); ++index) {
            const std::string& argument = arguments[index];
            if (argument == "-h" || argument == "--help") {
                factory_help();
                exit(EXIT_SUCCESS);
            }
            if (argument == "-d" || argument == "--dry-run") {
                result.dry_run = true;
            } else if (argument == "-f" || argument == "--force") {
                result.force = true;
            } else if (argument == "-S" || argument == "--with-slurm") {
                result.with_slurm = true;
            } else {
                ERROR("Unknown factory build option: " + argument);
                exit(EXIT_FAILURE);
            }
        }
        return result;
    }

    std::vector<std::filesystem::path> recipe_files(const std::filesystem::path& factory) {
        const std::filesystem::path recipes = factory / "recipes";
        if (!std::filesystem::is_directory(recipes)) {
            ERROR("Factory is missing recipes directory: " + factory.string());
            exit(EXIT_FAILURE);
        }

        std::vector<std::filesystem::path> files;
        for (const auto& entry : std::filesystem::directory_iterator(recipes)) {
            if (entry.is_regular_file() && entry.path().extension() == ".yaml") {
                files.push_back(entry.path());
            } else {
                WARNING("Skipping non-configuration file in recipes directory: " +
                        entry.path().string());
            }
        }
        std::sort(files.begin(), files.end());
        if (files.empty()) {
            ERROR("No recipe YAML files found in factory: " + factory.filename().string());
            exit(EXIT_FAILURE);
        }
        return files;
    }

    void run_install_plan(const BashCommandPlan& plan, const std::filesystem::path& prefix,
                          const FactoryBuildOptions& options) {
        if (options.dry_run) {
            print_command_plan(plan);
            return;
        }

        const std::filesystem::path plan_path =
            prefix / ".tmp" / ("factory-install-plan-" + std::to_string(::getpid()) + ".sh");
        write_install_plan(plan, plan_path);

        const std::filesystem::path script =
            std::filesystem::path(get_env_var("KEZ_HOME")) / "scripts" / "install.sh";
        if (!std::filesystem::is_regular_file(script)) {
            ERROR("Installation executor does not exist: " + script.string());
            exit(EXIT_FAILURE);
        }

        const unsigned int configured_jobs = load_user_config_parser_settings(prefix).parallel_jobs;
        const std::string install_jobs =
            get_env_var_noerr("KEZ_INSTALL_JOBS", std::to_string(configured_jobs));
        std::string command = "KEZ_INSTALL_JOBS=" + shell_single_quote(install_jobs) + " bash " +
                              shell_single_quote(script.string()) + " " +
                              shell_single_quote(prefix.string()) + " " +
                              shell_single_quote(plan_path.string());
        if (options.force) {
            command += " --force";
        }
        if (options.with_slurm) {
            command =
                "sbatch --wait --job-name=kez-factory-build --wrap=" + shell_single_quote(command);
        }
        run_external_command(command);

        std::error_code error;
        std::filesystem::remove(plan_path, error);
        if (error) {
            WARNING("Could not remove installation plan: " + error.message());
        }
    }

    void build_factory(const CommandArguments& arguments) {
        const FactoryBuildOptions options   = parse_build_options(arguments);
        const std::filesystem::path factory = active_factory_path();
        if (!std::filesystem::is_directory(factory)) {
            ERROR("Factory does not exist: " + factory.filename().string());
            exit(EXIT_FAILURE);
        }

        std::error_code error;
        std::filesystem::create_directories(factory / "buildspace", error);
        if (error) {
            ERROR("Failed to create factory buildspace directory: " + error.message());
            exit(EXIT_FAILURE);
        }

        for (const std::filesystem::path& recipe : recipe_files(factory)) {
            const std::string buildspace_name = recipe.stem().string();
            validate_path_component(buildspace_name, "buildspace name");
            const std::filesystem::path buildspace = factory / "buildspace" / buildspace_name;
            INFO("Building recipe: " + recipe.string());
            const BashCommandPlan plan = parse_cmdline(recipe, buildspace);
            run_install_plan(plan, buildspace, options);
        }
        SUCCESS(options.dry_run ? "Factory dry run completed." : "Factory build completed.");
    }

    std::filesystem::path factory_config_file(const std::filesystem::path& factory) {
        const std::filesystem::path current = factory / "runspace" / "config.yaml";
        if (std::filesystem::is_regular_file(current)) {
            return current;
        }
        const std::filesystem::path legacy = factory / "config.yaml";
        if (std::filesystem::is_regular_file(legacy)) {
            return legacy;
        }
        ERROR("Factory profile configuration not found: " + current.string());
        exit(EXIT_FAILURE);
    }

    FactoryPlan load_factory_plan(const std::filesystem::path& factory) {
        return parse_factory_config(YAML::LoadFile(factory_config_file(factory).string()));
    }

    void write_profile_script(const std::filesystem::path& script,
                              const std::filesystem::path& space,
                              const std::filesystem::path& buildspace,
                              const FactoryProfile& profile) {
        std::ofstream output(script, std::ios::out | std::ios::trunc);
        if (!output) {
            ERROR("Failed to create run script: " + script.string());
            exit(EXIT_FAILURE);
        }

        output << "#!/usr/bin/env bash\n"
                  "set -Eeuo pipefail\n"
               << "cd " << shell_single_quote(space.string()) << '\n'
               << "for kez_factory_bin in " << shell_single_quote((buildspace / "bin").string())
               << " " << shell_single_quote(buildspace.string())
               << "/*/bin; do\n"
                  "    if [[ -d \"$kez_factory_bin\" ]]; then\n"
                  "        export PATH=\"$kez_factory_bin:${PATH}\"\n"
                  "    fi\n"
                  "done\n"
                  "kez_factory_info() {\n"
                  "    if [[ -n \"${KEZ_HOME:-}\" && -x \"${KEZ_HOME}/bin/print_info\" ]]; then\n"
                  "        \"${KEZ_HOME}/bin/print_info\" \"$1\"\n"
                  "    else\n"
                  "        printf '%s\\n' \"$1\"\n"
                  "    fi\n"
                  "}\n";
        for (const std::string& command : profile.commands) {
            output << "kez_factory_info " << shell_single_quote("Executing: " + command) << '\n';
            output << command << '\n';
        }
        output.close();
        if (!output) {
            ERROR("Failed to write tasting script: " + script.string());
            exit(EXIT_FAILURE);
        }
    }

    void run_factory() {
        const std::filesystem::path factory = active_factory_path();
        if (!std::filesystem::is_directory(factory)) {
            ERROR("Factory does not exist: " + factory.filename().string());
            exit(EXIT_FAILURE);
        }
        const FactoryPlan plan               = load_factory_plan(factory);
        const std::filesystem::path runspace = factory / "runspace";
        std::error_code error;
        std::filesystem::create_directories(runspace, error);
        if (error) {
            ERROR("Failed to create runspace directory: " + error.message());
            exit(EXIT_FAILURE);
        }

        for (const FactoryBuildspace& buildspace_config : plan) {
            const std::filesystem::path buildspace =
                factory / "buildspace" / buildspace_config.name;
            if (!std::filesystem::is_directory(buildspace)) {
                ERROR("Factory buildspace does not exist: " + buildspace_config.name);
                exit(EXIT_FAILURE);
            }
            for (const FactoryProfile& profile : buildspace_config.profiles) {
                INFO("Processing buildspace: " + buildspace_config.name +
                     ", profile: " + profile.name);
                const std::filesystem::path space =
                    runspace / (buildspace_config.name + "_" + profile.name);
                std::filesystem::create_directories(space, error);
                if (error) {
                    ERROR("Failed to create runspace: " + error.message());
                    exit(EXIT_FAILURE);
                }
                const std::filesystem::path script = space / ".kez-profile.sh";
                write_profile_script(script, space, buildspace, profile);
                run_external_command("bash " + shell_single_quote(script.string()));
                std::filesystem::remove(script, error);
                if (error) {
                    WARNING("Could not remove tasting script: " + error.message());
                }
                SUCCESS("Completed buildspace: " + buildspace_config.name +
                        ", profile: " + profile.name);
            }
        }
        SUCCESS("Factory run completed.");
    }

    std::vector<std::filesystem::path> output_files(const std::filesystem::path& space) {
        std::vector<std::filesystem::path> result;
        for (const char* name : {"kez.out", "kez.err"}) {
            const std::filesystem::path path = space / name;
            if (std::filesystem::is_regular_file(path)) {
                result.push_back(path);
            }
        }
        return result;
    }

    void summarize_file(const std::filesystem::path& path, const std::regex& pattern,
                        bool& matched) {
        const std::vector<std::string> lines = split(read_file(path.string()), '\n');
        for (const std::string& line : lines) {
            if (std::regex_search(line, pattern)) {
                INFO(line);
                matched = true;
            }
        }
    }

    void summarize_factory() {
        const std::filesystem::path factory = active_factory_path();
        if (!std::filesystem::is_directory(factory)) {
            ERROR("Factory does not exist: " + factory.filename().string());
            exit(EXIT_FAILURE);
        }
        const FactoryPlan plan               = load_factory_plan(factory);
        const std::filesystem::path runspace = factory / "runspace";

        for (const FactoryBuildspace& buildspace : plan) {
            for (const FactoryProfile& profile : buildspace.profiles) {
                const std::filesystem::path space =
                    runspace / (buildspace.name + "_" + profile.name);
                const std::vector<std::filesystem::path> files = output_files(space);
                if (files.empty()) {
                    WARNING("No output files found for buildspace: " + buildspace.name +
                            ", profile: " + profile.name);
                    continue;
                }
                if (profile.summary_regex.empty()) {
                    WARNING("No summary regex defined for buildspace: " + buildspace.name +
                            ", profile: " + profile.name);
                    continue;
                }

                INFO("Summary for buildspace: " + buildspace.name + ", profile: " + profile.name);
                const std::regex pattern(profile.summary_regex);
                bool matched = false;
                for (const std::filesystem::path& file : files) {
                    summarize_file(file, pattern, matched);
                }
                if (!matched) {
                    WARNING("No matches found.");
                }
            }
        }
        SUCCESS("Factory summary completed.");
    }
}  // namespace

void execute_factory(const CommandArguments& arguments) {
    if (arguments.empty() || arguments.front() == "-h" || arguments.front() == "--help") {
        factory_help();
        return;
    }

    const std::string& action = arguments.front();
    if (action == "create") {
        create_factory(required_factory_name(arguments, action));
    } else if (action == "remove") {
        remove_factory(required_factory_name(arguments, action));
    } else if (action == "list") {
        if (arguments.size() != 1) {
            ERROR("factory list does not accept additional arguments");
            exit(EXIT_FAILURE);
        }
        list_directories(factories_root(), "factories");
    } else if (action == "enter") {
        const std::string name = required_factory_name(arguments, action);
        if (!get_env_var_noerr("KEZ_FACTORY").empty()) {
            ERROR("A factory is already selected: " + get_env_var_noerr("KEZ_FACTORY"));
            exit(EXIT_FAILURE);
        }
        if (!std::filesystem::is_directory(factory_path(name))) {
            ERROR("Factory does not exist: " + name);
            exit(EXIT_FAILURE);
        }
        std::cout << "export KEZ_FACTORY=" << shell_single_quote(name) << '\n';
    } else if (action == "exit") {
        if (arguments.size() != 1) {
            ERROR("factory exit does not accept additional arguments");
            exit(EXIT_FAILURE);
        }
        get_env_var("KEZ_FACTORY", "No factory is currently selected");
        std::cout << "unset KEZ_FACTORY\n";
    } else if (action == "which") {
        if (arguments.size() != 1) {
            ERROR("factory which does not accept additional arguments");
            exit(EXIT_FAILURE);
        }
        const std::string name = get_env_var_noerr("KEZ_FACTORY");
        INFO(name.empty() ? "No factory is currently selected." : "Current factory: " + name);
    } else if (action == "build") {
        build_factory(arguments);
    } else if (action == "run") {
        if (arguments.size() != 1) {
            ERROR("factory run does not accept additional arguments");
            exit(EXIT_FAILURE);
        }
        run_factory();
    } else if (action == "summarize") {
        if (arguments.size() != 1) {
            ERROR("factory summarize does not accept additional arguments");
            exit(EXIT_FAILURE);
        }
        summarize_factory();
    } else {
        ERROR("Unknown factory command: " + action);
        exit(EXIT_FAILURE);
    }
}
