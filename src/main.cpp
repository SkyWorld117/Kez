#include <stdlib.h>
#include <yaml-cpp/yaml.h>

#include <argparse/argparse.hpp>
#include <cmdline_parser/cmdline_parser.hpp>
#include <colors/colored_io.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <global_config.hpp>
#include <iostream>
#include <parser/parser.hpp>
#include <string>
#include <user_config_generator/user_config_generator.hpp>

void EXE_AND_CHECK(const std::string& cmd) {
    int ret = system(cmd.c_str());
    if (ret != 0) {
        ERROR("Command failed: " + cmd);
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char* argv[]) {
    argparse::ArgumentParser program("Fromager", "0.1.0");

    // ----------------------------------------------------------
    // Definition of arguments and subcommands
    // ----------------------------------------------------------

    // --- init ---
    argparse::ArgumentParser init_parser("init");
    init_parser.add_description("Initialize the Fromager environment");
    init_parser.add_argument("--refresh")
        .help("Refresh the Fromager environment by reinstalling core utilities")
        .default_value(false)
        .implicit_value(true);

    // --- selfcheck ---
    argparse::ArgumentParser selfcheck_parser("selfcheck");
    selfcheck_parser.add_description("Run self-checks on the Fromager installation");

    // --- utilities ---
    argparse::ArgumentParser utilities_parser("utilities");
    utilities_parser.add_description("Manage utilities");

    argparse::ArgumentParser util_add_parser("add");
    util_add_parser.add_description("Add a utility to the utilities cellar");
    auto& util_mutex_group = util_add_parser.add_mutually_exclusive_group();
    util_mutex_group.add_argument("-r", "--read")
        .help("Read a configuration file")
        .default_value(false)
        .implicit_value(true);
    util_mutex_group.add_argument("utility").help("Utility name").nargs(1);
    util_add_parser.add_argument("--config")
        .help("Additional configuration options for the utility in the format <option>=<value>")
        .nargs(argparse::nargs_pattern::at_least_one);

    argparse::ArgumentParser util_empty_parser("empty");
    util_empty_parser.add_description("Empty the utilities cellar");

    utilities_parser.add_subparser(util_add_parser);
    utilities_parser.add_subparser(util_empty_parser);

    // --- cellar ---
    argparse::ArgumentParser cellar_parser("cellar");
    cellar_parser.add_description("Manage per application environments");

    argparse::ArgumentParser cellar_create_parser("create");
    cellar_create_parser.add_description("Create a new cellar for an application");
    cellar_create_parser.add_argument("cellar_name");

    argparse::ArgumentParser cellar_remove_parser("remove");
    cellar_remove_parser.add_description("Remove an existing cellar");
    cellar_remove_parser.add_argument("cellar_name");

    argparse::ArgumentParser cellar_list_parser("list");
    cellar_list_parser.add_description("List all existing cellars");

    argparse::ArgumentParser cellar_enter_parser("enter");
    cellar_enter_parser.add_description("Enter a cellar to use its environment");
    cellar_enter_parser.add_argument("cellar_name");

    argparse::ArgumentParser cellar_exit_parser("exit");
    cellar_exit_parser.add_description("Exit the currently entered cellar");

    argparse::ArgumentParser cellar_which_parser("which");
    cellar_which_parser.add_description("Show the currently entered cellar");

    argparse::ArgumentParser cellar_empty_parser("empty");
    cellar_empty_parser.add_argument("cellar_name");

    cellar_parser.add_subparser(cellar_create_parser);
    cellar_parser.add_subparser(cellar_remove_parser);
    cellar_parser.add_subparser(cellar_list_parser);
    cellar_parser.add_subparser(cellar_enter_parser);
    cellar_parser.add_subparser(cellar_exit_parser);
    cellar_parser.add_subparser(cellar_which_parser);
    cellar_parser.add_subparser(cellar_empty_parser);

    // --- compiler & mpi ---
    argparse::ArgumentParser compiler_parser("compiler");
    compiler_parser.add_description("Manage compilers");

    argparse::ArgumentParser compiler_load_parser("load");
    compiler_load_parser.add_description("Load a compiler");
    compiler_load_parser.add_argument("compiler_name");

    argparse::ArgumentParser compiler_unload_parser("unload");
    compiler_unload_parser.add_description("Unload the currently loaded compiler");

    argparse::ArgumentParser compiler_list_parser("list");
    compiler_list_parser.add_description("List available compilers");

    argparse::ArgumentParser compiler_which_parser("which");
    compiler_which_parser.add_description("Show the currently loaded compiler");

    argparse::ArgumentParser compiler_remove_parser("remove");
    compiler_remove_parser.add_description("Remove a compiler from the system");
    compiler_remove_parser.add_argument("compiler_name");

    compiler_parser.add_subparser(compiler_load_parser);
    compiler_parser.add_subparser(compiler_unload_parser);
    compiler_parser.add_subparser(compiler_list_parser);
    compiler_parser.add_subparser(compiler_which_parser);
    compiler_parser.add_subparser(compiler_remove_parser);

    argparse::ArgumentParser mpi_parser("mpi");
    mpi_parser.add_description("Manage MPI implementations");

    argparse::ArgumentParser mpi_load_parser("load");
    mpi_load_parser.add_description("Load an MPI implementation");
    mpi_load_parser.add_argument("mpi_name");

    argparse::ArgumentParser mpi_unload_parser("unload");
    mpi_unload_parser.add_description("Unload the currently loaded MPI implementation");

    argparse::ArgumentParser mpi_list_parser("list");
    mpi_list_parser.add_description("List available MPI implementations");

    argparse::ArgumentParser mpi_which_parser("which");
    mpi_which_parser.add_description("Show the currently loaded MPI implementation");

    argparse::ArgumentParser mpi_remove_parser("remove");
    mpi_remove_parser.add_description("Remove an MPI implementation from the system");
    mpi_remove_parser.add_argument("mpi_name");

    mpi_parser.add_subparser(mpi_load_parser);
    mpi_parser.add_subparser(mpi_unload_parser);
    mpi_parser.add_subparser(mpi_list_parser);
    mpi_parser.add_subparser(mpi_which_parser);
    mpi_parser.add_subparser(mpi_remove_parser);

    // --- install ---
    argparse::ArgumentParser install_parser("install");
    install_parser.add_description("Install a package");
    install_parser.add_argument("-r", "--read")
        .help("Read a configuration file for installation")
        .default_value(false)
        .implicit_value(true);
    install_parser.add_argument("pkg_or_file")
        .help("Package name or config file if -r is used")
        .nargs(1);
    install_parser.add_argument("--config")
        .help("Configs to install with")
        .nargs(argparse::nargs_pattern::at_least_one);
    install_parser.add_argument("--cellar")
        .help("Target cellar (only used when installing a package that is not a "
              "compiler/MPI/vendor type)")
        .nargs(1);

    // --- template ---
    argparse::ArgumentParser template_parser("template");
    template_parser.add_description("Fetch a template for an application");

    argparse::ArgumentParser template_parse_parser("parse");
    template_parse_parser.add_description("Parse a user configuration file into instructions");
    template_parse_parser.add_argument("file");

    template_parser.add_argument("package").help("Package for template generation").nargs(1);
    template_parser.add_argument("-s", "--save").help("Save the configuration template").nargs(1);

    template_parser.add_subparser(template_parse_parser);

    // --- rt ---
    argparse::ArgumentParser rt_parser("rt");
    rt_parser.add_description("Manage rapid test factories");

    argparse::ArgumentParser rt_factory_parser("factory");
    rt_factory_parser.add_description("Manage rapid test factories");

    argparse::ArgumentParser rt_factory_create_parser("create");
    rt_factory_create_parser.add_description("Create a new rapid test factory");
    rt_factory_create_parser.add_argument("factory_name");

    argparse::ArgumentParser rt_factory_remove_parser("remove");
    rt_factory_remove_parser.add_description("Remove an existing rapid test factory");
    rt_factory_remove_parser.add_argument("factory_name");

    argparse::ArgumentParser rt_factory_list_parser("list");
    rt_factory_list_parser.add_description("List all existing rapid test factories");

    argparse::ArgumentParser rt_factory_enter_parser("enter");
    rt_factory_enter_parser.add_description("Enter a rapid test factory to use its environment");
    rt_factory_enter_parser.add_argument("factory_name");

    argparse::ArgumentParser rt_factory_exit_parser("exit");
    rt_factory_exit_parser.add_description("Exit the currently entered rapid test factory");

    argparse::ArgumentParser rt_factory_which_parser("which");
    rt_factory_which_parser.add_description("Show the currently entered rapid test factory");

    rt_factory_parser.add_subparser(rt_factory_create_parser);
    rt_factory_parser.add_subparser(rt_factory_remove_parser);
    rt_factory_parser.add_subparser(rt_factory_list_parser);
    rt_factory_parser.add_subparser(rt_factory_enter_parser);
    rt_factory_parser.add_subparser(rt_factory_exit_parser);
    rt_factory_parser.add_subparser(rt_factory_which_parser);

    argparse::ArgumentParser rt_build_parser("build");
    argparse::ArgumentParser rt_taste_parser("taste");
    argparse::ArgumentParser rt_summarize_parser("summarize");

    argparse::ArgumentParser rt_try_parser("try");
    rt_try_parser.add_argument("config");
    rt_try_parser.add_argument("package");

    rt_parser.add_subparser(rt_factory_parser);
    rt_parser.add_subparser(rt_build_parser);
    rt_parser.add_subparser(rt_taste_parser);
    rt_parser.add_subparser(rt_summarize_parser);
    rt_parser.add_subparser(rt_try_parser);

    // Register all subparsers
    program.add_subparser(init_parser);
    program.add_subparser(selfcheck_parser);
    program.add_subparser(utilities_parser);
    program.add_subparser(cellar_parser);
    program.add_subparser(compiler_parser);
    program.add_subparser(mpi_parser);
    program.add_subparser(install_parser);
    program.add_subparser(template_parser);
    program.add_subparser(rt_parser);

    try {
        program.parse_args(argc, argv);
    } catch (const std::exception& err) {
        ERROR(err.what());
        std::cerr << program;
        exit(EXIT_FAILURE);
    }

    // ----------------------------------------------------------
    // Handle subcommands and options
    // ----------------------------------------------------------

    // --- Handle version ---
    if (program.get<bool>("--version")) {
        INFO("Fromager version 0.1.0");
        return 0;
    }

    // --- Handle init ---
    if (program.is_subcommand_used("init")) {
        if (init_parser.get<bool>("--refresh")) {
            INFO("Refreshing Fromager system environment...");
            WARNING("Note: This will delete the existing system environment and recreate it.");
            EXE_AND_CHECK("${FROMAGER_HOME}/bin/fromager_init --refresh");
        } else {
            INFO("Initializing Fromager system environment...");
            EXE_AND_CHECK("${FROMAGER_HOME}/bin/fromager_init");
        }
        exit(EXIT_SUCCESS);
    }

    // --- Handle selfcheck ---
    if (program.is_subcommand_used("selfcheck")) {
        EXE_AND_CHECK("${FROMAGER_HOME}/bin/fromager_db_check");
        exit(EXIT_SUCCESS);
    }

    // --- Handle utilities ---
    if (program.is_subcommand_used("utilities")) {
        if (utilities_parser.is_subcommand_used("add")) {
            bool is_config_file = util_add_parser.get<bool>("--read");
            std::string target  = util_add_parser.get<std::string>("utility");

            CellarPathQuery query;
            // Set the `pkg_name` in the query to trigger the correct parsing logic in `get_cellar_path`
            // This should prevent non-regular packages to be installed
            if (is_config_file) {
                // TODO: Remember to change this part once we figure out how to handle multiple target packages in a single user config file.
                YAML::Node config          = YAML::LoadFile(target);
                std::string target_package = config["recipe"]["dependencies"][0].as<std::string>();
                query.pkg_name             = target_package;
            } else {
                query.pkg_name = target;
            }
            query.cellar_name            = "utilities";
            std::string utilities_cellar = get_cellar_path(query);

            if (util_add_parser.is_used("--config")) {
                std::vector<std::string> config_options =
                    util_add_parser.get<std::vector<std::string>>("--config");
                parse_cmdline(target, is_config_file, utilities_cellar, config_options);
            } else {
                parse_cmdline(target, is_config_file, utilities_cellar);
            }

            EXE_AND_CHECK("${FROMAGER_HOME}/bin/fromager_install " + utilities_cellar);

            exit(EXIT_SUCCESS);
        }

        if (utilities_parser.is_subcommand_used("empty")) {
            std::filesystem::path utilities_cellar = global_config::get_path("utilities");
            if (std::filesystem::exists(utilities_cellar)) {
                for (const auto& entry : std::filesystem::directory_iterator(utilities_cellar)) {
                    std::filesystem::remove_all(entry.path());
                }
                SUCCESS("Utilities cellar has been emptied.");
            } else {
                WARNING("Utilities cellar does not exist. Nothing to empty.");
            }

            exit(EXIT_SUCCESS);
        }
    }

    if (program.is_subcommand_used("cellar")) {
        if (cellar_parser.is_subcommand_used("create")) {
            std::string cellar_name = cellar_create_parser.get<std::string>("cellar_name");
            if (cellar_name.empty()) {
                ERROR("Cellar name cannot be empty.");
                exit(EXIT_FAILURE);
            }
            if (cellar_name == "system" || cellar_name == "compilers" || cellar_name == "mpis" ||
                cellar_name == "vendors" || cellar_name == "utilities") {
                ERROR("Cannot create cellar with reserved name: " + cellar_name);
                exit(EXIT_FAILURE);
            }
            std::filesystem::path cellar_path =
                global_config::get_path("cellars") + "/" + cellar_name;
            if (std::filesystem::exists(cellar_path)) {
                ERROR("Cellar already exists: " + cellar_name);
                exit(EXIT_FAILURE);
            }
            std::filesystem::create_directories(cellar_path);
            SUCCESS("Cellar created: " + cellar_name);

            exit(EXIT_SUCCESS);
        }

        if (cellar_parser.is_subcommand_used("remove")) {
            std::string cellar_name = cellar_remove_parser.get<std::string>("cellar_name");
            if (cellar_name.empty()) {
                ERROR("Cellar name cannot be empty.");
                exit(EXIT_FAILURE);
            }
            std::filesystem::path cellar_path =
                global_config::get_path("cellars") + "/" + cellar_name;
            if (!std::filesystem::exists(cellar_path)) {
                ERROR("Cellar does not exist: " + cellar_name);
                exit(EXIT_FAILURE);
            }
            std::filesystem::remove_all(cellar_path);
            SUCCESS("Cellar removed: " + cellar_name);

            exit(EXIT_SUCCESS);
        }

        if (cellar_parser.is_subcommand_used("list")) {
            std::filesystem::path cellars_path = global_config::get_path("cellars");
            if (!std::filesystem::exists(cellars_path)) {
                INFO("No cellars found.");
                exit(EXIT_SUCCESS);
            }
            INFO("Available cellars:");
            for (const auto& entry : std::filesystem::directory_iterator(cellars_path)) {
                if (entry.is_directory() && entry.path().filename() != "system" &&
                    entry.path().filename() != "compilers" && entry.path().filename() != "mpis" &&
                    entry.path().filename() != "vendors" &&
                    entry.path().filename() != "utilities") {
                    std::cout << "  - " << entry.path().filename().string() << std::endl;
                }
            }

            exit(EXIT_SUCCESS);
        }

        if (cellar_parser.is_subcommand_used("enter")) {
            std::string cellar_name = cellar_enter_parser.get<std::string>("cellar_name");
            if (cellar_name.empty()) {
                ERROR("Cellar name cannot be empty.");
                exit(EXIT_FAILURE);
            }
            if (cellar_name == "system" || cellar_name == "compilers" || cellar_name == "mpis" ||
                cellar_name == "vendors" || cellar_name == "utilities") {
                ERROR("Cannot enter reserved cellar: " + cellar_name);
                exit(EXIT_FAILURE);
            }
            std::filesystem::path cellar_path =
                global_config::get_path("cellars") + "/" + cellar_name;
            if (!std::filesystem::exists(cellar_path)) {
                ERROR("Cellar does not exist: " + cellar_name);
                exit(EXIT_FAILURE);
            }
            // Print out the command to set PATH for the entered cellar. The main shell script will evaluate this output and update the environment accordingly.
            std::cout << "export PATH=\"" << cellar_path.string()
                      << "/bin:$PATH\"; export FROMAGER_CELLAR=\"" << cellar_name << "\""
                      << std::endl;

            exit(EXIT_SUCCESS);
        }

        if (cellar_parser.is_subcommand_used("exit")) {
            // Print out the command to unset PATH for the exited cellar. The main shell script will evaluate this output and update the environment accordingly.
            std::string cellar_name =
                std::getenv("FROMAGER_CELLAR") ? std::getenv("FROMAGER_CELLAR") : "";
            if (cellar_name.empty()) {
                ERROR("No cellar is currently entered.");
                exit(EXIT_FAILURE);
            }
            if (cellar_name == "system" || cellar_name == "compilers" || cellar_name == "mpis" ||
                cellar_name == "vendors" || cellar_name == "utilities") {
                ERROR("Cannot exit reserved cellar: " + cellar_name);
                exit(EXIT_FAILURE);
            }
            std::filesystem::path cellar_path =
                global_config::get_path("cellars") + "/" + cellar_name;
            std::cout << "export PATH=\"$(echo $PATH | sed -e 's;" + cellar_path.string() +
                             "/bin:;;g')\"; unset FROMAGER_CELLAR"
                      << std::endl;

            exit(EXIT_SUCCESS);
        }

        if (cellar_parser.is_subcommand_used("which")) {
            std::string cellar_name =
                std::getenv("FROMAGER_CELLAR") ? std::getenv("FROMAGER_CELLAR") : "";
            if (cellar_name.empty()) {
                INFO("Not currently in a cellar.");
            } else {
                INFO("Currently in cellar: " + cellar_name);
            }

            exit(EXIT_SUCCESS);
        }

        if (cellar_parser.is_subcommand_used("empty")) {
            std::string cellar_name = cellar_empty_parser.get<std::string>("cellar_name");
            if (cellar_name.empty()) {
                ERROR("Cellar name cannot be empty.");
                exit(EXIT_FAILURE);
            }
            if (cellar_name == "system" || cellar_name == "compilers" || cellar_name == "mpis" ||
                cellar_name == "vendors" || cellar_name == "utilities") {
                ERROR("Cannot empty reserved cellar: " + cellar_name);
                exit(EXIT_FAILURE);
            }
            std::filesystem::path cellar_path =
                global_config::get_path("cellars") + "/" + cellar_name;
            if (!std::filesystem::exists(cellar_path)) {
                ERROR("Cellar does not exist: " + cellar_name);
                exit(EXIT_FAILURE);
            }
            for (const auto& entry : std::filesystem::directory_iterator(cellar_path)) {
                std::filesystem::remove_all(entry.path());
            }
            SUCCESS("Cellar has been emptied: " + cellar_name);

            exit(EXIT_SUCCESS);
        }
    }

    if (program.is_subcommand_used("compiler")) {
        if (compiler_parser.is_subcommand_used("load")) {
            std::string compiler_name = compiler_load_parser.get<std::string>("compiler_name");
            if (compiler_name.empty()) {
                ERROR("Compiler name cannot be empty.");
                exit(EXIT_FAILURE);
            }
            std::filesystem::path compiler_path =
                global_config::get_path("compilers") + "/" + compiler_name;
            if (!std::filesystem::exists(compiler_path)) {
                ERROR("Compiler does not exist: " + compiler_name);
                exit(EXIT_FAILURE);
            }
            // Print out the command to set PATH for the loaded compiler. The main shell script will evaluate this output and update the environment accordingly.
            std::cout << "export PATH=\"" << compiler_path.string()
                      << "/bin:$PATH\"; export FROMAGER_COMPILER=\"" << compiler_name << "\""
                      << std::endl;

            exit(EXIT_SUCCESS);
        }

        if (compiler_parser.is_subcommand_used("unload")) {
            // Print out the command to unset PATH for the unloaded compiler. The main shell script will evaluate this output and update the environment accordingly.
            std::string compiler_name =
                std::getenv("FROMAGER_COMPILER") ? std::getenv("FROMAGER_COMPILER") : "";
            if (compiler_name.empty()) {
                ERROR("No compiler is currently loaded.");
                exit(EXIT_FAILURE);
            }
            std::filesystem::path compiler_path =
                global_config::get_path("compilers") + "/" + compiler_name;
            std::cout << "export PATH=\"$(echo $PATH | sed -e 's;" + compiler_path.string() +
                             "/bin:;;g')\"; unset FROMAGER_COMPILER"
                      << std::endl;

            exit(EXIT_SUCCESS);
        }

        if (compiler_parser.is_subcommand_used("list")) {
            std::filesystem::path compilers_path = global_config::get_path("compilers");
            if (!std::filesystem::exists(compilers_path)) {
                INFO("No compilers found.");
                exit(EXIT_SUCCESS);
            }
            INFO("Available compilers:");
            for (const auto& entry : std::filesystem::directory_iterator(compilers_path)) {
                if (entry.is_directory()) {
                    std::cout << "  - " << entry.path().filename().string() << std::endl;
                }
            }

            exit(EXIT_SUCCESS);
        }

        if (compiler_parser.is_subcommand_used("which")) {
            std::string compiler_name =
                std::getenv("FROMAGER_COMPILER") ? std::getenv("FROMAGER_COMPILER") : "";
            if (compiler_name.empty()) {
                INFO("No compiler is currently loaded.");
            } else {
                INFO("Currently loaded compiler: " + compiler_name);
            }

            exit(EXIT_SUCCESS);
        }

        if (compiler_parser.is_subcommand_used("remove")) {
            std::string compiler_name = compiler_remove_parser.get<std::string>("compiler_name");
            if (compiler_name.empty()) {
                ERROR("Compiler name cannot be empty.");
                exit(EXIT_FAILURE);
            }
            std::filesystem::path compiler_path =
                global_config::get_path("compilers") + "/" + compiler_name;
            if (!std::filesystem::exists(compiler_path)) {
                ERROR("Compiler does not exist: " + compiler_name);
                exit(EXIT_FAILURE);
            }
            std::filesystem::remove_all(compiler_path);
            SUCCESS("Compiler removed: " + compiler_name);

            exit(EXIT_SUCCESS);
        }
    }

    if (program.is_subcommand_used("mpi")) {
        // Similar handling for MPI implementations can be implemented here following the pattern of compilers
        if (mpi_parser.is_subcommand_used("load")) {
            std::string mpi_name = mpi_load_parser.get<std::string>("mpi_name");
            if (mpi_name.empty()) {
                ERROR("MPI implementation name cannot be empty.");
                exit(EXIT_FAILURE);
            }
            std::filesystem::path mpi_path = global_config::get_path("mpis") + "/" + mpi_name;
            if (!std::filesystem::exists(mpi_path)) {
                ERROR("MPI implementation does not exist: " + mpi_name);
                exit(EXIT_FAILURE);
            }
            // Print out the command to set PATH for the loaded MPI implementation. The main shell script will evaluate this output and update the environment accordingly.
            std::cout << "export PATH=\"" << mpi_path.string()
                      << "/bin:$PATH\"; export FROMAGER_MPI=\"" << mpi_name << "\"" << std::endl;

            exit(EXIT_SUCCESS);
        }

        if (mpi_parser.is_subcommand_used("unload")) {
            // Print out the command to unset PATH for the unloaded MPI implementation. The main shell script will evaluate this output and update the environment accordingly.
            std::string mpi_name = std::getenv("FROMAGER_MPI") ? std::getenv("FROMAGER_MPI") : "";
            if (mpi_name.empty()) {
                ERROR("No MPI implementation is currently loaded.");
                exit(EXIT_FAILURE);
            }
            std::filesystem::path mpi_path = global_config::get_path("mpis") + "/" + mpi_name;
            std::cout << "export PATH=\"$(echo $PATH | sed -e 's;" + mpi_path.string() +
                             "/bin:;;g')\"; unset FROMAGER_MPI"
                      << std::endl;

            exit(EXIT_SUCCESS);
        }

        if (mpi_parser.is_subcommand_used("list")) {
            std::filesystem::path mpis_path = global_config::get_path("mpis");
            if (!std::filesystem::exists(mpis_path)) {
                INFO("No MPI implementations found.");
                exit(EXIT_SUCCESS);
            }
            INFO("Available MPI implementations:");
            for (const auto& entry : std::filesystem::directory_iterator(mpis_path)) {
                if (entry.is_directory()) {
                    std::cout << "  - " << entry.path().filename().string() << std::endl;
                }
            }

            exit(EXIT_SUCCESS);
        }

        if (mpi_parser.is_subcommand_used("which")) {
            std::string mpi_name = std::getenv("FROMAGER_MPI") ? std::getenv("FROMAGER_MPI") : "";
            if (mpi_name.empty()) {
                INFO("No MPI implementation is currently loaded.");
            } else {
                INFO("Currently loaded MPI implementation: " + mpi_name);
            }

            exit(EXIT_SUCCESS);
        }

        if (mpi_parser.is_subcommand_used("remove")) {
            std::string mpi_name = mpi_remove_parser.get<std::string>("mpi_name");
            if (mpi_name.empty()) {
                ERROR("MPI implementation name cannot be empty.");
                exit(EXIT_FAILURE);
            }
            std::filesystem::path mpi_path = global_config::get_path("mpis") + "/" + mpi_name;
            if (!std::filesystem::exists(mpi_path)) {
                ERROR("MPI implementation does not exist: " + mpi_name);
                exit(EXIT_FAILURE);
            }
            std::filesystem::remove_all(mpi_path);
            SUCCESS("MPI implementation removed: " + mpi_name);

            exit(EXIT_SUCCESS);
        }
    }

    if (program.is_subcommand_used("install")) {
        bool is_config_file = install_parser.get<bool>("--read");
        std::string target  = install_parser.get<std::string>("pkg_or_file");

        CellarPathQuery query;
        if (is_config_file) {
            YAML::Node config          = YAML::LoadFile(target);
            std::string target_package = config["recipe"]["dependencies"][0].as<std::string>();
            query.pkg_name             = target_package;
        } else {
            query.pkg_name = target;
        }
        if (install_parser.is_used("--cellar")) {
            query.cellar_name = install_parser.get<std::string>("--cellar");
        }
        std::string target_cellar = get_cellar_path(query);

        if (install_parser.is_used("--config")) {
            std::vector<std::string> config_options =
                install_parser.get<std::vector<std::string>>("--config");
            parse_cmdline(target, is_config_file, target_cellar, config_options);
        } else {
            parse_cmdline(target, is_config_file, target_cellar);
        }

        EXE_AND_CHECK("${FROMAGER_HOME}/bin/fromager_install " + target_cellar);

        exit(EXIT_SUCCESS);
    }

    if (program.is_subcommand_used("template")) {
        if (template_parser.is_subcommand_used("parse")) {
            std::string file  = template_parse_parser.get<std::string>("file");
            YAML::Node config = YAML::LoadFile(file);

            std::filesystem::path tmp_path =
                std::filesystem::path(getenv("FROMAGER_WORKDIR")) / ".tmp";
            std::filesystem::create_directories(tmp_path);

            YAML::Node instructions_yaml = parse(config, "release", tmp_path.string());
            YAML::Emitter out;
            out << instructions_yaml;
            std::ofstream ofs((tmp_path / "ins.yaml").string());
            if (!ofs) {
                ERROR("Failed to create instruction file");
                exit(EXIT_FAILURE);
            }
            ofs << out.c_str();
            ofs.close();

            SUCCESS("Instructions written to: " + (tmp_path / "ins.yaml").string());

            exit(EXIT_SUCCESS);
        } else {
            std::string package = template_parser.get<std::string>("package");
            bool save_template  = template_parser.is_used("--save");

            YAML::Node user_config = gen_user_config(package, save_template);
            YAML::Emitter out;
            out << user_config;

            std::cout << out.c_str() << std::endl;

            if (save_template) {
                std::string output_file = template_parser.get<std::string>("--save");
                std::ofstream ofs(output_file);
                if (!ofs) {
                    ERROR("Could not open output file: " + output_file);
                    exit(EXIT_FAILURE);
                }
                ofs << out.c_str();
                ofs.close();
                SUCCESS("Configuration template written to: " + output_file);
            } else {
                SUCCESS("Configuration template output to stdout.");
            }

            exit(EXIT_SUCCESS);
        }
    }

    if (program.is_subcommand_used("rt")) {
        if (rt_parser.is_subcommand_used("factory")) {
            if (rt_factory_parser.is_subcommand_used("create")) {
                std::string factory_name =
                    rt_factory_create_parser.get<std::string>("factory_name");
                if (factory_name.empty()) {
                    ERROR("Factory name cannot be empty.");
                    exit(EXIT_FAILURE);
                }
                std::filesystem::path factory_path =
                    global_config::get_path("factories") + "/" + factory_name;
                if (std::filesystem::exists(factory_path)) {
                    ERROR("Factory already exists: " + factory_name);
                    exit(EXIT_FAILURE);
                }
                std::filesystem::create_directories(factory_path);
                std::filesystem::create_directories(factory_path / "wheels");
                std::filesystem::create_directories(factory_path / "tasting_rooms");
                SUCCESS("Factory created: " + factory_name);
            }

            if (rt_factory_parser.is_subcommand_used("remove")) {
                std::string factory_name =
                    rt_factory_remove_parser.get<std::string>("factory_name");
                if (factory_name.empty()) {
                    ERROR("Factory name cannot be empty.");
                    exit(EXIT_FAILURE);
                }
                std::filesystem::path factory_path =
                    global_config::get_path("factories") + "/" + factory_name;
                if (!std::filesystem::exists(factory_path)) {
                    ERROR("Factory does not exist: " + factory_name);
                    exit(EXIT_FAILURE);
                }
                std::filesystem::remove_all(factory_path);
                SUCCESS("Factory removed: " + factory_name);
            }

            if (rt_factory_parser.is_subcommand_used("list")) {
                std::filesystem::path factories_path = global_config::get_path("factories");
                if (!std::filesystem::exists(factories_path)) {
                    INFO("No factories found.");
                    exit(EXIT_SUCCESS);
                }
                INFO("Available factories:");
                for (const auto& entry : std::filesystem::directory_iterator(factories_path)) {
                    if (entry.is_directory()) {
                        std::cout << "  - " << entry.path().filename().string() << std::endl;
                    }
                }
            }

            if (rt_factory_parser.is_subcommand_used("enter")) {
                std::string factory_name = rt_factory_enter_parser.get<std::string>("factory_name");
                if (factory_name.empty()) {
                    ERROR("Factory name cannot be empty.");
                    exit(EXIT_FAILURE);
                }
                std::filesystem::path factory_path =
                    global_config::get_path("factories") + "/" + factory_name;
                if (!std::filesystem::exists(factory_path)) {
                    ERROR("Factory does not exist: " + factory_name);
                    exit(EXIT_FAILURE);
                }
                std::cout << "export FROMAGER_FACTORY=\"" << factory_name << "\"" << std::endl;
            }

            if (rt_factory_parser.is_subcommand_used("exit")) {
                std::string factory_name =
                    std::getenv("FROMAGER_FACTORY") ? std::getenv("FROMAGER_FACTORY") : "";
                if (factory_name.empty()) {
                    ERROR("No factory is currently entered.");
                    exit(EXIT_FAILURE);
                }
                std::cout << "unset FROMAGER_FACTORY" << std::endl;
            }

            if (rt_factory_parser.is_subcommand_used("which")) {
                std::string factory_name =
                    std::getenv("FROMAGER_FACTORY") ? std::getenv("FROMAGER_FACTORY") : "";
                if (factory_name.empty()) {
                    INFO("Not currently in a factory.");
                } else {
                    INFO("Currently in factory: " + factory_name);
                }
            }

            exit(EXIT_SUCCESS);
        }

        if (rt_parser.is_subcommand_used("build")) {
            INFO("Starting rapid test build process...");
            if (std::getenv("FROMAGER_FACTORY") == nullptr) {
                ERROR("No factory is currently entered. Please enter a factory to use its "
                      "environment for building.");
                exit(EXIT_FAILURE);
            }
            std::string factory_name = std::getenv("FROMAGER_FACTORY");
            std::filesystem::path factory_path =
                global_config::get_path("factories") + "/" + factory_name;

            if (!std::filesystem::exists(factory_path)) {
                ERROR("Factory does not exist: " + factory_name);
                exit(EXIT_FAILURE);
            }
            if (!std::filesystem::exists(factory_path / "wheels")) {
                ERROR("Factory is missing 'wheels' directory: " + factory_name);
                exit(EXIT_FAILURE);
            }
            std::filesystem::path wheels_path = factory_path / "wheels";
            if (std::filesystem::is_empty(wheels_path)) {
                ERROR("No wheels found in factory: " + factory_name);
                exit(EXIT_FAILURE);
            }

            for (const auto& entry : std::filesystem::directory_iterator(wheels_path)) {
                if (entry.is_regular_file() && entry.path().extension() == ".yaml") {
                    std::string config_path = entry.path().string();
                    INFO("Installing from configuration: " + config_path);
                    YAML::Node config = YAML::LoadFile(config_path);
                    std::filesystem::path target_cellar =
                        factory_path / "cellar" / entry.path().stem();

                    YAML::Node instructions_yaml = parse(config, "release", target_cellar.string());
                    YAML::Emitter out;
                    out << instructions_yaml;
                    std::filesystem::path tmp_path = target_cellar / ".tmp";
                    std::filesystem::create_directories(tmp_path);
                    std::ofstream ofs((tmp_path / "ins.yaml").string());
                    if (!ofs) {
                        ERROR("Failed to create instruction file for config: " + config_path);
                        exit(EXIT_FAILURE);
                    }
                    ofs << out.c_str();
                    ofs.close();

                    EXE_AND_CHECK("${FROMAGER_HOME}/bin/fromager_install " +
                                  target_cellar.string());

                    SUCCESS("Installation from configuration " + config_path + " completed.");
                } else {
                    WARNING("Skipping non-configuration file in wheels directory: " +
                            entry.path().string());
                }
            }

            SUCCESS("Rapid test build process completed.");
            exit(EXIT_SUCCESS);
        }

        if (rt_parser.is_subcommand_used("taste")) {
            INFO("Tasting the cheese...");
            if (std::getenv("FROMAGER_FACTORY") == nullptr) {
                ERROR("No factory is currently entered. Please enter a factory to use its "
                      "environment for tasting.");
                exit(EXIT_FAILURE);
            }
            std::string factory_name = std::getenv("FROMAGER_FACTORY");
            std::filesystem::path factory_path =
                global_config::get_path("factories") + "/" + factory_name;

            if (!std::filesystem::exists(factory_path)) {
                ERROR("Factory does not exist: " + factory_name);
                exit(EXIT_FAILURE);
            }

            if (!std::filesystem::exists(factory_path / "tasting_rooms")) {
                ERROR("Factory is missing 'tasting_rooms' directory: " + factory_name);
                exit(EXIT_FAILURE);
            }
            std::filesystem::path tasting_rooms_path = factory_path / "tasting_rooms";
            if (!std::filesystem::exists(tasting_rooms_path / "config.yaml")) {
                ERROR("Factory is missing tasting room configuration: " + factory_name);
                exit(EXIT_FAILURE);
            }

            EXE_AND_CHECK("${FROMAGER_HOME}/bin/fromager_rt_profile");
            SUCCESS("Tasting completed.");
        }

        if (rt_parser.is_subcommand_used("summarize")) {
            INFO("Summarizing the results...");
            if (std::getenv("FROMAGER_FACTORY") == nullptr) {
                ERROR("No factory is currently entered. Please enter a factory to use its "
                      "environment for summarizing.");
                exit(EXIT_FAILURE);
            }
            std::string factory_name = std::getenv("FROMAGER_FACTORY");
            std::filesystem::path factory_path =
                global_config::get_path("factories") + "/" + factory_name;

            if (!std::filesystem::exists(factory_path)) {
                ERROR("Factory does not exist: " + factory_name);
                exit(EXIT_FAILURE);
            }

            if (!std::filesystem::exists(factory_path / "tasting_rooms")) {
                ERROR("Factory is missing 'tasting_rooms' directory: " + factory_name);
                exit(EXIT_FAILURE);
            }
            std::filesystem::path tasting_rooms_path = factory_path / "tasting_rooms";

            EXE_AND_CHECK("${FROMAGER_HOME}/bin/fromager_rt_summarize");
            SUCCESS("Tasting summary completed.");
        }

        if (rt_parser.is_subcommand_used("try")) {
            if (std::getenv("FROMAGER_CELLAR") == nullptr) {
                ERROR("No cellar is currently entered. Please enter a cellar to use its "
                      "environment for trying the configuration.");
                exit(EXIT_FAILURE);
            }
            std::string cellar_name = std::getenv("FROMAGER_CELLAR");

            std::string config_path = rt_try_parser.get<std::string>("config");
            std::string package     = rt_try_parser.get<std::string>("package");

            if (!std::filesystem::exists(config_path)) {
                ERROR("Configuration file does not exist: " + config_path);
                exit(EXIT_FAILURE);
            }

            std::filesystem::path target_cellar =
                global_config::get_path("cellars") + "/" + cellar_name;
            std::filesystem::path tmp_path = target_cellar / ".tmp";
            std::filesystem::create_directories(tmp_path);
            YAML::Node config            = YAML::LoadFile(config_path);
            YAML::Node instructions_yaml = parse(config, "debug", tmp_path.string());

            YAML::Emitter out;
            out << instructions_yaml;
            std::ofstream ofs((tmp_path / "ins.yaml").string());
            if (!ofs) {
                ERROR("Failed to create instruction file for config: " + config_path);
                exit(EXIT_FAILURE);
            }
            ofs << out.c_str();
            ofs.close();

            EXE_AND_CHECK("${FROMAGER_HOME}/bin/fromager_rt_install " + target_cellar.string() +
                          " " + config_path + " " + package);

            SUCCESS("Rapid test try process completed.");
            exit(EXIT_SUCCESS);
        }
    }

    return 0;
}
