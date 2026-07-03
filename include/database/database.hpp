#pragma once

#include <database/config.hpp>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>

class DatabaseError : public std::runtime_error {
   public:
    using std::runtime_error::runtime_error;
};

class ConfigError : public DatabaseError {
   public:
    using DatabaseError::DatabaseError;
};

using PackageConfigPtr = std::shared_ptr<const PackageConfig>;

PackageConfigPtr get_db_config(const std::string& package_name, const std::string& version);
PackageConfigPtr get_db_config(const std::string& package_name);

PackageConfigPtr parse_db_config(const std::filesystem::path& config_path);

void clear_db_cache();
