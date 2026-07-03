#pragma once

#include <filesystem>
#include <string>

std::filesystem::path select_config_path(const std::filesystem::path& database_path,
                                         const std::string& package_name,
                                         const std::string& version);
void validate_package_name(const std::string& package_name);
