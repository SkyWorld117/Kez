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
#include <ui/argparse.hpp>
#include <ui/commands.hpp>
#include <ui/factory.hpp>
#include <ui/install.hpp>
#include <ui/ui_utils.hpp>
#include <utils/bash_utils.hpp>
#include <utils/colored_io.hpp>
#include <utils/file_utils.hpp>
#include <utils/string_utils.hpp>
#include <utils/yaml_utils.hpp>
#include <vector>

std::string render_factory_profile_script(const std::filesystem::path& space,
                                          const std::filesystem::path& buildspace,
                                          const FactoryProfile& profile) {
    std::string result =
        "#!/usr/bin/env bash\n"
        "set -Eeuo pipefail\n"
        "cd " +
        shell_single_quote(space.string()) + "\nfor kez_factory_bin in " +
        shell_single_quote((buildspace / "bin").string()) + " " +
        shell_single_quote(buildspace.string()) +
        "/*/bin; do\n"
        "    if [[ -d \"$kez_factory_bin\" ]]; then\n"
        "        export PATH=\"$kez_factory_bin:${PATH}\"\n"
        "    fi\n"
        "done\n"
        "kez_factory_info() {\n"
        "    if [[ -n \"${KEZ_HOME:-}\" && -x \"${KEZ_HOME}/bin/kez_print\" ]]; then\n"
        "        \"${KEZ_HOME}/bin/kez_print\" info \"$1\"\n"
        "    else\n"
        "        printf '%s\\n' \"$1\"\n"
        "    fi\n"
        "}\n";
    for (const std::string& command : profile.commands) {
        result += "kez_factory_info " + shell_single_quote("Executing: " + command) + "\n";
        result += command + "\n";
    }
    return result;
}

std::vector<std::string> matching_factory_summary_lines(const std::string& contents,
                                                        const std::regex& pattern) {
    std::vector<std::string> result;
    for (const std::string& line : split(contents, '\n')) {
        if (std::regex_search(line, pattern)) {
            result.push_back(line);
        }
    }
    return result;
}

namespace {
    /**
     * @brief Prints the factory subcommand usage message to stdout.
     *
     * Lists all valid factory commands (create, remove, list, enter, exit,
     * which, build, run, summarize) and their build-specific options
     * (--dry-run, --force, --with-slurm).  Does not terminate the program;
     * the caller is expected to return or exit as appropriate.
     */
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

    /** @brief Returns the root directory under which all factories are stored. */
    std::filesystem::path factories_root() { return configured_work_path("factories"); }

    /** @brief Returns the filesystem path for the factory with the given name. */
    std::filesystem::path factory_path(const std::string& name) {
        validate_path_component(name, "factory name");
        return factories_root() / name;
    }

    /** @brief Returns the path of the currently selected (active) factory. */
    std::filesystem::path active_factory_path() {
        const std::string name = get_env_var(
            "KEZ_FACTORY", "No factory is currently selected. Run 'kez factory enter <name>'.");
        return factory_path(name);
    }

    /** @brief Creates a new empty factory directory tree (recipes, buildspace, runspace). */
    void create_factory(const std::string& name) {
        const std::filesystem::path path = factory_path(name);
        if (fs_exists(path)) {
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

    /** @brief Removes an existing factory directory tree. */
    void remove_factory(const std::string& name) {
        const std::filesystem::path path = factory_path(name);
        if (!fs_directory(path)) {
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

    /** @brief Collects and sorts the YAML recipe files from a factory's recipes directory. */
    std::vector<std::filesystem::path> recipe_files(const std::filesystem::path& factory) {
        const std::filesystem::path recipes = factory / "recipes";
        if (!fs_directory(recipes)) {
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

    /**
     * @brief Executes (or dry-runs) a BashCommandPlan inside a factory buildspace.
     *
     * When not a dry run, writes the plan to a temporary file and invokes
     * scripts/install.sh, optionally wrapping the invocation in sbatch when
     * --with-slurm is requested.
     */
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
        if (!fs_regular_file(script)) {
            ERROR("Installation executor does not exist: " + script.string());
            exit(EXIT_FAILURE);
        }

        run_external_command(install_executor_command(script, prefix, plan_path, options.force,
                                                      options.with_slurm, "kez-factory-build"));

        std::error_code error;
        std::filesystem::remove(plan_path, error);
        if (error) {
            WARNING("Could not remove installation plan: " + error.message());
        }
    }

    /** @brief Installs every recipe in the currently active factory. */
    void build_factory(const FactoryBuildOptions& options) {
        const std::filesystem::path factory = active_factory_path();
        if (!fs_directory(factory)) {
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

    /**
     * @brief Locates a factory's runspace config.yaml, falling back to a legacy
     *        top-level config.yaml if the runspace one does not exist.
     */
    std::filesystem::path factory_config_file(const std::filesystem::path& factory) {
        const std::filesystem::path current = factory / "runspace" / "config.yaml";
        if (fs_regular_file(current)) {
            return current;
        }
        const std::filesystem::path legacy = factory / "config.yaml";
        if (fs_regular_file(legacy)) {
            return legacy;
        }
        ERROR("Factory profile configuration not found: " + current.string());
        exit(EXIT_FAILURE);
    }

    /** @brief Parses a factory's runspace config.yaml into a FactoryPlan. */
    FactoryPlan load_factory_plan(const std::filesystem::path& factory) {
        return parse_factory_config(load_yaml_file(factory_config_file(factory)));
    }

    /**
     * @brief Writes a profile run-script that sets up PATH and executes the
     *        profile's commands.
     */
    void write_profile_script(const std::filesystem::path& script,
                              const std::filesystem::path& space,
                              const std::filesystem::path& buildspace,
                              const FactoryProfile& profile) {
        std::ofstream output(script, std::ios::out | std::ios::trunc);
        if (!output) {
            ERROR("Failed to create run script: " + script.string());
            exit(EXIT_FAILURE);
        }

        output << render_factory_profile_script(space, buildspace, profile);
        output.close();
        if (!output) {
            ERROR("Failed to write tasting script: " + script.string());
            exit(EXIT_FAILURE);
        }
    }

    /** @brief Runs every profile defined in the currently active factory's runspace config. */
    void run_factory() {
        const std::filesystem::path factory = active_factory_path();
        if (!fs_directory(factory)) {
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
            if (!fs_directory(buildspace)) {
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

    /** @brief Returns the kez.out and kez.err output files that exist under the given space. */
    std::vector<std::filesystem::path> output_files(const std::filesystem::path& space) {
        std::vector<std::filesystem::path> result;
        for (const char* name : {"kez.out", "kez.err"}) {
            const std::filesystem::path path = space / name;
            if (fs_regular_file(path)) {
                result.push_back(path);
            }
        }
        return result;
    }

    /** @brief Searches a single output file for lines matching a regex and prints matches. */
    void summarize_file(const std::filesystem::path& path, const std::regex& pattern,
                        bool& matched) {
        for (const std::string& line :
             matching_factory_summary_lines(read_file(path.string()), pattern)) {
            INFO(line);
            matched = true;
        }
    }

    /** @brief Summarises output from every buildspace/profile combination in the active factory. */
    void summarize_factory() {
        const std::filesystem::path factory = active_factory_path();
        if (!fs_directory(factory)) {
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

/** @brief Dispatches factory subcommands (create, remove, list, enter, exit, which, build, run, summarize). */
void execute_factory(const CommandArguments& arguments) {
    const FactoryArgumentsParseResult parsed = parse_factory_arguments(arguments);
    if (!parsed.error.empty()) {
        ERROR(parsed.error);
        exit(EXIT_FAILURE);
    }

    switch (parsed.arguments.action) {
        case FactoryAction::Help: factory_help(); return;
        case FactoryAction::Create: create_factory(parsed.arguments.name); return;
        case FactoryAction::Remove: remove_factory(parsed.arguments.name); return;
        case FactoryAction::List: list_directories(factories_root(), "factories"); return;
        case FactoryAction::Enter: {
            const std::string& name = parsed.arguments.name;
            if (!get_env_var_noerr("KEZ_FACTORY").empty()) {
                ERROR("A factory is already selected: " + get_env_var_noerr("KEZ_FACTORY"));
                exit(EXIT_FAILURE);
            }
            if (!fs_directory(factory_path(name))) {
                ERROR("Factory does not exist: " + name);
                exit(EXIT_FAILURE);
            }
            std::cout << "export KEZ_FACTORY=" << shell_single_quote(name) << '\n';
            return;
        }
        case FactoryAction::Exit:
            get_env_var("KEZ_FACTORY", "No factory is currently selected");
            std::cout << "unset KEZ_FACTORY\n";
            return;
        case FactoryAction::Which: {
            const std::string name = get_env_var_noerr("KEZ_FACTORY");
            INFO(name.empty() ? "No factory is currently selected." : "Current factory: " + name);
            return;
        }
        case FactoryAction::Build: build_factory(parsed.arguments.build_options); return;
        case FactoryAction::Run: run_factory(); return;
        case FactoryAction::Summarize: summarize_factory(); return;
    }
}
