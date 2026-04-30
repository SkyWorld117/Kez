#pragma once

#include <argparse/argparse.hpp>
#include <cmdline_parser/cmdline_parser.hpp>
#include <global_config.hpp>
#include <string>
#include <utils/colored_io.hpp>

inline void EXE_AND_CHECK(const std::string& cmd) {
    int ret = system(cmd.c_str());
    if (ret != 0) {
        ERROR("Command failed: " + cmd);
        exit(EXIT_FAILURE);
    }
}

argparse::ArgumentParser& get_init_parser();
void execute_init_parser();
std::vector<std::string> get_init_suggestions(const int comp_cword,
                                              const std::vector<std::string>& comp_words);

argparse::ArgumentParser& get_selfcheck_parser();
void execute_selfcheck_parser();
std::vector<std::string> get_selfcheck_suggestions(const int comp_cword,
                                                   const std::vector<std::string>& comp_words);

argparse::ArgumentParser& get_utilities_parser();
void execute_utilities_parser();
std::vector<std::string> get_utilities_suggestions(const int comp_cword,
                                                   const std::vector<std::string>& comp_words);

argparse::ArgumentParser& get_update_parser();
void execute_update_parser();
std::vector<std::string> get_update_suggestions(const int comp_cword,
                                                const std::vector<std::string>& comp_words);

argparse::ArgumentParser& get_cellar_parser();
void execute_cellar_parser();
std::vector<std::string> get_cellar_suggestions(const int comp_cword,
                                                const std::vector<std::string>& comp_words);

argparse::ArgumentParser& get_compiler_parser();
void execute_compiler_parser();
std::vector<std::string> get_compiler_suggestions(const int comp_cword,
                                                  const std::vector<std::string>& comp_words);

argparse::ArgumentParser& get_mpi_parser();
void execute_mpi_parser();
std::vector<std::string> get_mpi_suggestions(const int comp_cword,
                                             const std::vector<std::string>& comp_words);

argparse::ArgumentParser& get_install_parser();
void execute_install_parser();
std::vector<std::string> get_install_suggestions(const int comp_cword,
                                                 const std::vector<std::string>& comp_words);

argparse::ArgumentParser& get_template_parser();
void execute_template_parser();
std::vector<std::string> get_template_suggestions(const int comp_cword,
                                                  const std::vector<std::string>& comp_words);

argparse::ArgumentParser& get_rt_parser();
void execute_rt_parser();
std::vector<std::string> get_rt_suggestions(const int comp_cword,
                                            const std::vector<std::string>& comp_words);

argparse::ArgumentParser& get_info_parser();
void execute_info_parser();
std::vector<std::string> get_info_suggestions(const int comp_cword,
                                              const std::vector<std::string>& comp_words);