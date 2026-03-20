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
#include <string>

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
    auto& util_mutually_exclusive_group = util_add_parser.add_mutually_exclusive_group();
    util_mutually_exclusive_group.add_argument("-r", "--read")
        .help("Read a configuration file")
        .required()
        .nargs(1);
    util_mutually_exclusive_group.add_argument("utility").help("Utility name").nargs(1);
    util_add_parser.add_argument("--config")
        .help("Additional configuration options for the utility in the format <option>=<value>")
        .nargs(argparse::nargs_pattern::at_least_one);

    argparse::ArgumentParser util_empty_parser("empty");

    utilities_parser.add_subparser(util_add_parser);
    utilities_parser.add_subparser(util_empty_parser);

    // --- cellar ---
    argparse::ArgumentParser cellar_parser("cellar");
    cellar_parser.add_description("Manage per application environments");

    argparse::ArgumentParser cellar_add_parser("add");
    cellar_add_parser.add_argument("cellar_name");
    argparse::ArgumentParser cellar_create_parser("create");
    cellar_create_parser.add_argument("cellar_name");

    argparse::ArgumentParser cellar_rm_parser("rm");
    cellar_rm_parser.add_argument("cellar_name");
    argparse::ArgumentParser cellar_remove_parser("remove");
    cellar_remove_parser.add_argument("cellar_name");

    argparse::ArgumentParser cellar_ls_parser("ls");
    argparse::ArgumentParser cellar_list_parser("list");

    argparse::ArgumentParser cellar_enter_parser("enter");
    cellar_enter_parser.add_argument("cellar_name");

    argparse::ArgumentParser cellar_exit_parser("exit");
    argparse::ArgumentParser cellar_which_parser("which");

    argparse::ArgumentParser cellar_empty_parser("empty");
    cellar_empty_parser.add_argument("cellar_name");

    cellar_parser.add_subparser(cellar_add_parser);
    cellar_parser.add_subparser(cellar_create_parser);
    cellar_parser.add_subparser(cellar_rm_parser);
    cellar_parser.add_subparser(cellar_remove_parser);
    cellar_parser.add_subparser(cellar_ls_parser);
    cellar_parser.add_subparser(cellar_list_parser);
    cellar_parser.add_subparser(cellar_enter_parser);
    cellar_parser.add_subparser(cellar_exit_parser);
    cellar_parser.add_subparser(cellar_which_parser);
    cellar_parser.add_subparser(cellar_empty_parser);

    // --- mpi & compiler ---
    argparse::ArgumentParser load_mpi_parser("load-mpi");
    load_mpi_parser.add_argument("mpi");
    argparse::ArgumentParser unload_mpi_parser("unload-mpi");

    argparse::ArgumentParser load_compiler_parser("load-compiler");
    load_compiler_parser.add_argument("compiler");
    argparse::ArgumentParser unload_compiler_parser("unload-compiler");

    // --- install ---
    argparse::ArgumentParser install_parser("install");
    install_parser.add_description("Install a package");
    install_parser.add_argument("-r", "--read")
        .help("Read requirements from config and install in [cellar]")
        .default_value(false)
        .implicit_value(true);
    install_parser.add_argument("--config").help("Configs to install with");
    install_parser.add_argument("--cellar").help("Target cellar");
    install_parser.add_argument("pkg_or_file").help("Package name or config file if -r is used");

    // --- template ---
    argparse::ArgumentParser template_parser("template");
    template_parser.add_description("Fetch a template for an application");

    argparse::ArgumentParser template_parse_parser("parse");
    template_parse_parser.add_argument("file");

    template_parser.add_argument("-s", "--save").help("Save the configuration template");
    template_parser.add_argument("package").help("Package for template generation").remaining();

    template_parser.add_subparser(template_parse_parser);

    // --- rt ---
    argparse::ArgumentParser rt_parser("rt");
    rt_parser.add_description("Manage rapid test factories");

    argparse::ArgumentParser rt_factory_parser("factory");
    rt_factory_parser.add_argument("action").help(
        "add, create, rm, remove, ls, list, enter, exit, which");
    rt_factory_parser.add_argument("factory_name").remaining();

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
    program.add_subparser(load_mpi_parser);
    program.add_subparser(unload_mpi_parser);
    program.add_subparser(load_compiler_parser);
    program.add_subparser(unload_compiler_parser);
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
            std::string target;
            bool is_config_file;
            if (util_add_parser.is_used("--read")) {
                target         = util_add_parser.get<std::string>("--read");
                is_config_file = true;
            } else {
                target         = util_add_parser.get<std::string>("utility");
                is_config_file = false;
            }

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
        }

        if (utilities_parser.is_subcommand_used("empty")) {
            std::filesystem::path utilities_cellar = global_config::get_path("utilities");
            if (std::filesystem::exists(utilities_cellar)) {
                std::filesystem::remove_all(utilities_cellar);
                SUCCESS("Utilities cellar has been emptied.");
            } else {
                WARNING("Utilities cellar does not exist. Nothing to empty.");
            }
        }
    }

    return 0;
}
