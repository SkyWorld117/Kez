#pragma once

#include <database/config.hpp>
#include <optional>
#include <string>
#include <vector>

/** @brief Display row for one user-configurable option or environment value. */
struct PackageConfigEntry {
    std::string name;
    std::optional<std::string> default_value;
    std::string description;
};

/** @brief Collect user-configurable entries from top-level and stage configurations. */
std::vector<PackageConfigEntry> package_config_entries(const PackageConfig& package);

/** @brief Return the user-facing name of a package toolchain. */
std::string toolchain_name(Toolchain toolchain);

/** @brief Return the user-facing name of a package source type. */
std::string source_type_name(SourceType source_type);

/** @brief Return the default display value of a package property. */
std::string property_display_value(const Property& property);
