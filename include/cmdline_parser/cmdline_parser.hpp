#pragma once

#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <parser/user_config_parser.hpp>
#include <string>
#include <vector>

void apply_cmdline_config(YAML::Node user_config, const std::vector<std::string>& config_options);

BashCommandPlan parse_cmdline(const std::filesystem::path& file,
                              const std::filesystem::path& install_prefix,
                              const std::vector<std::string>& config_options = {});
BashCommandPlan parse_cmdline(const std::vector<std::string>& targets,
                              const std::filesystem::path& install_prefix,
                              const std::vector<std::string>& config_options = {});
BashCommandPlan parse_cmdline(YAML::Node user_config, const std::filesystem::path& install_prefix,
                              const std::vector<std::string>& config_options = {});

// Serialize a typed command plan as shell function calls understood only by scripts/install.sh.
// This is an execution boundary, not a second package configuration format.
void write_install_plan(const BashCommandPlan& plan, const std::filesystem::path& path);
