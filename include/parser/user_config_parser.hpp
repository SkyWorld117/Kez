#pragma once

#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

enum class UserConfigBuildMode {
    Release,
    Debug,
};

struct ExternalPackageSettings {
    std::string prefix;
    std::string version;
};

struct UserConfigParserSettings {
    UserConfigBuildMode build_mode = UserConfigBuildMode::Release;
    std::filesystem::path install_prefix;
    std::filesystem::path kez_home;
    std::filesystem::path system_prefix;
    std::filesystem::path compilers_prefix;
    std::filesystem::path mpis_prefix;
    std::filesystem::path vendors_prefix;
    unsigned int parallel_jobs = 1;
    std::string architecture;
    std::unordered_map<std::string, std::string> architecture_variants;
    std::unordered_map<std::string, ExternalPackageSettings> external_packages;
};

struct PackageCommands {
    std::string package;
    std::vector<std::string> commands;
};

using BashCommandPlan = std::vector<PackageCommands>;

UserConfigParserSettings load_user_config_parser_settings(
    const std::filesystem::path& install_prefix, UserConfigBuildMode build_mode);

BashCommandPlan parse_user_config(const YAML::Node& user_config,
                                  const UserConfigParserSettings& settings);

BashCommandPlan parse_user_config(const YAML::Node& user_config, const std::string& build_mode,
                                  const std::filesystem::path& install_prefix);

BashCommandPlan parse_user_config_file(const std::filesystem::path& path,
                                       const UserConfigParserSettings& settings);
