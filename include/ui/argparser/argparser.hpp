#pragma once

#include <argparse/argparse.hpp>
#include <cmdline_parser/cmdline_parser.hpp>
#include <colors/colored_io.hpp>
#include <global_config.hpp>

inline void EXE_AND_CHECK(const std::string& cmd) {
    int ret = system(cmd.c_str());
    if (ret != 0) {
        ERROR("Command failed: " + cmd);
        exit(EXIT_FAILURE);
    }
}

argparse::ArgumentParser& get_init_parser();
void execute_init_parser();

argparse::ArgumentParser& get_selfcheck_parser();
void execute_selfcheck_parser();

argparse::ArgumentParser& get_utilities_parser();
void execute_utilities_parser();

argparse::ArgumentParser& get_cellar_parser();
void execute_cellar_parser();

argparse::ArgumentParser& get_compiler_parser();
void execute_compiler_parser();

argparse::ArgumentParser& get_mpi_parser();
void execute_mpi_parser();

argparse::ArgumentParser& get_install_parser();
void execute_install_parser();

argparse::ArgumentParser& get_template_parser();
void execute_template_parser();

argparse::ArgumentParser& get_rt_parser();
void execute_rt_parser();