#include <yaml-cpp/yaml.h>

#include <cmdline_parser/traverse.hpp>
#include <colors/colored_io.hpp>
#include <database/database.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <parser/parser.hpp>
#include <string>
#include <user_config_generator/user_config_generator.hpp>

void parse_cmdline(const std::string& target, bool is_config_file, const std::string& cellar_path);
void parse_cmdline(const std::string& target, bool is_config_file, const std::string& cellar_path,
                   const std::vector<std::string>& config_options);