#include <yaml-cpp/yaml.h>

#include <cmdline_parser/traverse.hpp>
#include <utils/colored_io.hpp>
#include <database/database.hpp>
#include <parser/parser.hpp>
#include <string>
#include <user_config_generator/user_config_generator.hpp>

void parse_cmdline(const std::string& file, const std::string& cellar_path,
                   const std::vector<std::string>& config_options);
void parse_cmdline(const std::vector<std::string>& target, const std::string& cellar_path,
                   const std::vector<std::string>& config_options);

void parse_cmdline(YAML::Node user_config, const std::string& cellar_path,
                   const std::vector<std::string>& config_options);