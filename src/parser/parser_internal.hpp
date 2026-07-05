#pragma once

#include <yaml-cpp/yaml.h>

#include <database/database.hpp>
#include <parser/user_config_parser.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct ParsedUserPackage {
    std::string requested_name;
    YAML::Node user_config;
    PackageConfigPtr database_config;
};

struct ParsedOptionState {
    bool enabled = true;
    std::string enabled_value;
    std::string disabled_value;

    const std::string& selected_value() const { return enabled ? enabled_value : disabled_value; }
};

struct UserConfigParserContext {
    YAML::Node user_config;
    UserConfigParserSettings settings;
    std::vector<ParsedUserPackage> packages;
    std::unordered_map<std::string, std::size_t> package_indices;
    std::unordered_map<std::string, std::string> package_aliases;
    std::unordered_map<std::string, PackageConfigPtr> extra_configs;
    std::unordered_map<std::string, std::string> abstract_packages;
    std::unordered_set<std::string> dependencies;
    std::unordered_map<const EnvironmentVariable*, std::string> environment_values;
    std::unordered_map<const BuildOption*, ParsedOptionState> option_values;
    std::unordered_map<std::string, std::string> named_environment_values;
    std::unordered_map<std::string, ParsedOptionState> named_option_values;
    std::unordered_set<std::string> resolving_templates;
    std::string current_package;
};

[[noreturn]] void user_config_error(const std::string& message);

std::string apply_parser_conditions(const ConfigurableValue<std::string>& configurable,
                                    const std::string& base_value,
                                    UserConfigParserContext& context);
bool apply_parser_conditions(const ConfigurableValue<bool>& configurable, bool base_value,
                             UserConfigParserContext& context);

std::string resolve_parser_scalar(const std::string& value, UserConfigParserContext& context);
std::string parser_package_prefix(const std::string& package_name,
                                  UserConfigParserContext& context);

void append_source_commands(const ParsedUserPackage& package, UserConfigParserContext& context,
                            std::vector<std::string>& commands);
