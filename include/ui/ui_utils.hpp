#pragma once

#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <string>
#include <vector>

std::filesystem::path configured_work_path(const std::string& name);
std::filesystem::path installation_prefix(const YAML::Node& user_config,
                                          const std::string& environment_name, bool utilities);

void validate_path_component(const std::string& value, const std::string& description);
void run_external_command(const std::string& command);
void list_directories(const std::filesystem::path& root, const std::string& heading);
void emit_environment_activation(const std::filesystem::path& prefix, const std::string& variable,
                                 const std::string& value);
void emit_environment_deactivation(const std::filesystem::path& prefix,
                                   const std::string& variable);

std::vector<std::string> user_config_targets(const YAML::Node& user_config);
