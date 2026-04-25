#include <stdlib.h>
#include <yaml-cpp/yaml.h>

#include <argparse/argparse.hpp>
#include <cmdline_parser/cmdline_parser.hpp>
#include <utils/colored_io.hpp>
#include <cstdlib>
#include <global_config.hpp>
#include <iostream>
#include <parser/parser.hpp>
#include <string>
#include <ui/argparser/argparser.hpp>
#include <user_config_generator/user_config_generator.hpp>

int main(int argc, char* argv[]) {
    argparse::ArgumentParser program("Fromager", "0.1.0");

    // ----------------------------------------------------------
    // Definition of arguments and subcommands
    // ----------------------------------------------------------

    argparse::ArgumentParser& init_parser      = get_init_parser();
    argparse::ArgumentParser& selfcheck_parser = get_selfcheck_parser();
    argparse::ArgumentParser& utilities_parser = get_utilities_parser();
    argparse::ArgumentParser& cellar_parser    = get_cellar_parser();
    argparse::ArgumentParser& compiler_parser  = get_compiler_parser();
    argparse::ArgumentParser& mpi_parser       = get_mpi_parser();
    argparse::ArgumentParser& install_parser   = get_install_parser();
    argparse::ArgumentParser& template_parser  = get_template_parser();
    argparse::ArgumentParser& rt_parser        = get_rt_parser();
    argparse::ArgumentParser& update_parser    = get_update_parser();

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
    program.add_subparser(update_parser);

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

    if (program.is_subcommand_used("init")) {
        execute_init_parser();
    }

    if (program.is_subcommand_used("selfcheck")) {
        execute_selfcheck_parser();
    }

    if (program.is_subcommand_used("update")) {
        execute_update_parser();
    }

    if (program.is_subcommand_used("utilities")) {
        execute_utilities_parser();
    }

    if (program.is_subcommand_used("cellar")) {
        execute_cellar_parser();
    }

    if (program.is_subcommand_used("compiler")) {
        execute_compiler_parser();
    }

    if (program.is_subcommand_used("mpi")) {
        execute_mpi_parser();
    }

    if (program.is_subcommand_used("install")) {
        execute_install_parser();
    }

    if (program.is_subcommand_used("template")) {
        execute_template_parser();
    }

    if (program.is_subcommand_used("rt")) {
        execute_rt_parser();
    }

    return 0;
}
