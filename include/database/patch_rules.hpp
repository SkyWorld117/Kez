#pragma once

#include <database/config.hpp>
#include <filesystem>
#include <string>
#include <vector>

/**
 * @brief One version constraint from a package patch rule.
 *
 * Supported operators are ``==``, ``>=``, ``>``, ``<=``, and ``<``.
 */
struct PatchVersionConstraint {
    /** @brief Comparison operator applied to the selected package version. */
    std::string operation;
    /** @brief Version on the right-hand side of the comparison. */
    std::string version;
};

/**
 * @brief Describes a package patch and its generated default state.
 *
 * Every constraint in @ref versions must match for the rule to apply. An empty
 * constraint list makes the patch applicable to every version.
 */
struct PatchRule {
    /** @brief Basename of the patch file inside the package's patch directory. */
    std::string name;
    /** @brief Default enabled state emitted into generated user configuration. */
    bool enabled = false;
    /** @brief Version constraints that delimit where the patch is applicable. */
    std::vector<PatchVersionConstraint> versions;
};

/**
 * @brief Return the patch directory belonging to a parsed package recipe.
 *
 * @param package Parsed package configuration.
 * @return ``<recipe-directory>/patches``.
 */
std::filesystem::path package_patch_directory(const PackageConfig& package);

/**
 * @brief Parse and validate all rules in a package-local patch directory.
 *
 * A missing patch directory is valid and returns an empty vector. When the
 * directory exists, ``_rules.yaml`` is required. Every regular patch file must
 * have exactly one rule, and every rule must name an existing regular file.
 *
 * @param package_directory Directory containing the package recipe YAML files.
 * @return Validated rules sorted by patch filename.
 *
 * @warning Terminates through @c ERROR() on malformed rules or filesystem layout.
 */
std::vector<PatchRule> load_patch_rules(const std::filesystem::path& package_directory);

/**
 * @brief Return rules that apply to a selected package version.
 *
 * @param package Parsed package configuration carrying its source recipe path.
 * @param version Concrete selected version.
 * @return Applicable rules sorted by patch filename.
 */
std::vector<PatchRule> applicable_patch_rules(const PackageConfig& package,
                                              const std::string& version);
