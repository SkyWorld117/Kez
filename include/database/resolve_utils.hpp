#pragma once

/**
 * @file resolve_utils.hpp
 * @brief Package-config resolution helpers shared across the generator and
 *        parser pipelines.
 *
 * Functions here extend the low-level get_db_config() with two common
 * look-up patterns:
 *   - Resolving the special "compiler" package name to the configured
 *     compiler's recipe (reading the system gcc version from manifest.yaml
 *     when the compiler spec is "system").
 *   - Resolving abstract package names to their selected concrete
 *     implementation.
 *
 * @see get_db_config()  in database/database.hpp
 * @see has_property()   in database/config.hpp
 */

#include <yaml-cpp/yaml.h>

#include <database/config.hpp>
#include <database/database.hpp>
#include <dependency_resolver/resolve_dependencies.hpp>
#include <filesystem>
#include <string>
#include <utility>
#include <utils/bash_utils.hpp>
#include <utils/yaml_utils.hpp>

/**
 * @brief Parse a compiler specification string into a (name, version) pair.
 *
 * If the specification is empty or "system", reads the system gcc version
 * from manifest.yaml.  Otherwise splits on '@' to separate the compiler
 * name from its version.  If no '@' is present, the version defaults to
 * "latest".
 *
 * @param compiler  The compiler specification (e.g. "gcc@13.4.0",
 *                  "llvm", "system", or empty).
 * @return A pair of (compiler_name, compiler_version).
 */
inline std::pair<std::string, std::string> parse_compiler(const std::string& compiler) {
    if (compiler.empty() || compiler == "system") {
        const std::string kez_home = get_env_var("KEZ_HOME");
        const auto manifest_path   = std::filesystem::path(kez_home) / "manifest.yaml";
        const YAML::Node manifest  = cached_yaml_load(manifest_path);
        return {"gcc", yaml_scalar(manifest["system-stack"]["gcc"], "system-stack.gcc")};
    }
    const std::size_t separator = compiler.find('@');
    if (separator == std::string::npos) {
        return {compiler, "latest"};
    }
    return {compiler.substr(0, separator), compiler.substr(separator + 1)};
}

/**
 * @brief Resolve a package name to its configuration, handling the special
 *        "compiler" case and abstract-package lookups.
 *
 * If @p package is the literal string "compiler", the function returns the
 * compiler's own recipe by calling get_db_config() with the result of
 * parse_compiler(@p compiler).  Otherwise it consults @p abstract_packages:
 * if an entry exists the concrete name is used, falling back to @p package
 * as-is.
 *
 * @param package            The package name to resolve (may be "compiler"
 *                           or an abstract package name).
 * @param abstract_packages  Map from abstract package names to concrete
 *                           implementations.
 * @param compiler           Compiler specification string (passed through
 *                           to parse_compiler when @p package == "compiler").
 * @return The resolved PackageConfigPtr.
 */
inline PackageConfigPtr property_config(const std::string& package,
                                        const AbstractPackageSelections& abstract_packages,
                                        const std::string& compiler) {
    if (package == "compiler") {
        const auto [compiler_name, compiler_version] = parse_compiler(compiler);
        return get_db_config(compiler_name, compiler_version);
    }
    const auto abstract = abstract_packages.find(package);
    return get_db_config(abstract == abstract_packages.end() ? package : abstract->second);
}

/**
 * @brief Check whether a resolved package declares a named property,
 *        handling abstract-package and "compiler" resolution.
 *
 * Convenience wrapper around property_config() + has_property().  See
 * property_config() for the resolution semantics.
 *
 * @param package            The package name to check.
 * @param property           The property name (e.g. "includes", "ldflags").
 * @param abstract_packages  Map from abstract package names to concrete
 *                           implementations.
 * @param compiler           Compiler specification string (passed through
 *                           to property_config).
 * @return true if the resolved package declares the property (or an alias
 *         matching it per has_property()'s rules).
 *
 * @see has_property()  For the alias rules applied during lookup.
 */
inline bool has_template_property(const std::string& package, const std::string& property,
                                  const AbstractPackageSelections& abstract_packages,
                                  const std::string& compiler) {
    return has_property(*property_config(package, abstract_packages, compiler), property);
}
