/**
 * @file template_resolver.cpp
 * @brief Resolution of package properties, versions, prefixes, and template values.
 */

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cstdlib>
#include <database/config.hpp>
#include <filesystem>
#include <parser/parser_internal.hpp>
#include <string>
#include <utility>
#include <utils/colored_io.hpp>
#include <utils/yaml_utils.hpp>
#include <variant>

/** @brief Prints an invalid-user-configuration error message and terminates the program.
 *  @warning This function does not return; it exits with EXIT_FAILURE. */
[[noreturn]] void user_config_error(const std::string& message) {
    ERROR("Invalid user configuration: " + message);
    exit(EXIT_FAILURE);
}

namespace {

    /**
     * @brief Resolve a package name through the alias map.
     *
     * If @p package_name is registered as an alias in
     * @p context.package_aliases, the canonical (target) name is returned;
     * otherwise @p package_name is returned unchanged.
     *
     * @param context       Parser context holding the alias map
     *                      (@ref UserConfigParserContext::package_aliases).
     * @param package_name  The package name as written by the user (may be an
     *                      alias).
     * @return The canonical package name after alias resolution.
     */
    std::string canonical_package_name(UserConfigParserContext& context,
                                       const std::string& package_name) {
        const auto alias = context.package_aliases.find(package_name);
        return alias == context.package_aliases.end() ? package_name : alias->second;
    }

}  // namespace

/** @brief Retrieve package metadata through parser aliases and caches. */
PackageConfigPtr parser_package_config(UserConfigParserContext& context,
                                       const std::string& package_name) {
    const std::string requested_name = canonical_package_name(context, package_name);
    const auto parsed                = context.package_indices.find(requested_name);
    if (parsed != context.package_indices.end()) {
        return context.packages[parsed->second].database_config;
    }

    const auto cached = context.extra_configs.find(requested_name);
    if (cached != context.extra_configs.end()) {
        return cached->second;
    }
    PackageConfigPtr config = get_db_config(requested_name);
    context.extra_configs.emplace(requested_name, config);
    if (config->name != requested_name) {
        context.package_aliases.emplace(config->name, requested_name);
    }
    return config;
}

namespace {

    /** @brief Retrieves the user's YAML configuration node for a given package name. */
    YAML::Node parser_user_package(UserConfigParserContext& context,
                                   const std::string& package_name) {
        const std::string requested_name = canonical_package_name(context, package_name);
        const auto parsed                = context.package_indices.find(requested_name);
        if (parsed == context.package_indices.end()) {
            return YAML::Node();
        }
        return context.packages[parsed->second].user_config;
    }

    /** @brief Removes the "${" and "}" delimiters from a template string if present. */
    std::string strip_template(const std::string& value) {
        if (value.size() >= 3 && value.rfind("${", 0) == 0 && value.back() == '}') {
            return value.substr(2, value.size() - 3);
        }
        return value;
    }
    /** @brief Returns the name-version pair of the compiler selected for the current package. */
    std::pair<std::string, std::string> current_compiler(UserConfigParserContext& context) {
        YAML::Node user_package   = parser_user_package(context, context.current_package);
        std::string specification = "system";
        if (yaml_has(user_package, "compiler")) {
            specification = yaml_scalar(user_package["compiler"], "package compiler");
        }
        if (specification == "system") {
            return {"gcc", "system"};
        }
        const std::size_t separator = specification.find('@');
        if (separator == std::string::npos || separator == 0 ||
            separator + 1 == specification.size()) {
            user_config_error("compiler specification '" + specification +
                              "' must be 'system' or '<name>@<version>'");
        }
        return {specification.substr(0, separator), specification.substr(separator + 1)};
    }

    /** @brief Checks whether a compiler name corresponds to an NVIDIA compiler (nvhpc, nvc, or nvcc). */
    bool is_nvidia_compiler(const std::string& compiler) {
        return compiler.find("nvhpc") != std::string::npos || compiler == "nvc" ||
               compiler == "nvcc";
    }

    /** @brief Formats a filesystem path as a -I compiler flag. */
    std::string format_include_path(const std::string& path) { return "-I" + path; }

    /** @brief Formats a library path as -L and -Xlinker -rpath flags for NVIDIA compilers. */
    std::string format_nvidia_library_path(const std::string& path) {
        return "-L" + path + " -Xlinker -rpath," + path;
    }

    /** @brief Formats a library path as -L and -rpath flags, using NVIDIA-specific syntax when the active compiler is an NVIDIA compiler. */
    std::string format_library_path(const std::string& path, UserConfigParserContext& context) {
        return is_nvidia_compiler(current_compiler(context).first)
                   ? format_nvidia_library_path(path)
                   : "-L" + path + " -Wl,-rpath," + path;
    }

    /** @brief Applies parser conditions to a ConfigurableValue<std::string>, returning the resulting string. */
    std::string apply_string_value(const ConfigurableValue<std::string>& configurable,
                                   UserConfigParserContext& context) {
        const std::string base = configurable.default_value.value_or("");
        return apply_parser_conditions(configurable, base, context);
    }

    /** @brief Resolves a declared property value from a PackageConfig, applying conditions and recursive template resolution. */
    std::string resolve_declared_property(const std::string& template_name,
                                          const PackageConfig& config,
                                          const std::string& property_name,
                                          UserConfigParserContext& context) {
        const Property* property = find_property(config, property_name);
        if (property == nullptr) {
            user_config_error("property '" + property_name + "' does not exist for package '" +
                              config.name + "'");
        }
        if (!context.resolving_templates.emplace(template_name).second) {
            user_config_error("cyclic template reference involving '" + template_name + "'");
        }

        std::string value;
        if (std::holds_alternative<std::string>(property->data)) {
            value = std::get<std::string>(property->data);
        } else {
            value = apply_string_value(std::get<ConfigurableValue<std::string>>(property->data),
                                       context);
        }
        value = resolve_parser_scalar(value, context);
        context.resolving_templates.erase(template_name);
        return value;
    }

    /** @brief Resolves the version string for a package from user config, database config, or state file. */
    std::string parser_package_version(const std::string& package_name,
                                       UserConfigParserContext& context) {
        const std::string requested_name = canonical_package_name(context, package_name);
        YAML::Node user_package          = parser_user_package(context, requested_name);
        if (yaml_has(user_package, "version")) {
            std::string version = yaml_scalar(user_package["version"], "package version");
            const std::size_t local_separator = version.find('@');
            if (local_separator != std::string::npos) {
                version = version.substr(0, local_separator);
            }
            return version;
        }

        const PackageConfigPtr config = parser_package_config(context, requested_name);
        if (config->type == PackageType::External) {
            const auto external = context.settings.external_packages.find(requested_name);
            if (external == context.settings.external_packages.end() ||
                external->second.version.empty()) {
                user_config_error("external package '" + requested_name +
                                  "' has no configured version");
            }
            return external->second.version;
        }
        if (config->type == PackageType::System) {
            const std::filesystem::path state_path = context.settings.system_prefix / "state.yaml";
            if (!std::filesystem::is_regular_file(state_path)) {
                user_config_error("system package state file does not exist: " +
                                  state_path.string());
            }
            const YAML::Node state = YAML::LoadFile(state_path.string());
            if (yaml_has(state, "state") && yaml_has(state["state"], requested_name)) {
                YAML::Node value = state["state"][requested_name];
                return value.IsMap() && yaml_has(value, "version")
                           ? yaml_scalar(value["version"], "system package version")
                           : yaml_scalar(value, "system package version");
            }
            if (yaml_has(state, "kez") && yaml_has(state["kez"], requested_name) &&
                yaml_has(state["kez"][requested_name], "version")) {
                return yaml_scalar(state["kez"][requested_name]["version"],
                                   "system package version");
            }
            user_config_error("system package '" + requested_name + "' is absent from state.yaml");
        }

        const auto [compiler_name, compiler_version] = current_compiler(context);
        if (requested_name == compiler_name) {
            return compiler_version;
        }
        // If the requested package is the "parent" of the current compiler
        // (e.g. nvhpc is the parent of nvhpc-compilers), use the compiler's
        // version rather than the first source release of the parent.
        if (compiler_version != "system" && compiler_version != "latest") {
            const PackageConfigPtr compiler_config = parser_package_config(context, compiler_name);
            const Property* parent_prop            = find_property(*compiler_config, "parent");
            if (parent_prop != nullptr && std::holds_alternative<std::string>(parent_prop->data)) {
                const std::string& parent_name = std::get<std::string>(parent_prop->data);
                if (parent_name == requested_name) {
                    return compiler_version;
                }
            }
        }
        if (config->source.has_value() && !config->source->releases.empty()) {
            return config->source->releases.front().version;
        }
        user_config_error("package '" + requested_name + "' has no version");
    }

    /** @brief Resolves a template name to its base value without applying overrides. */
    std::string resolve_parser_template_base(const std::string& name,
                                             UserConfigParserContext& context) {
        if (name == "source") {
            return (context.settings.install_prefix / ".tmp" / context.current_package / "source")
                .string();
        }
        if (name == "kez.prefix") {
            return context.settings.install_prefix.string();
        }
        if (name == "kez.arch") {
            const auto selected = context.settings.architecture_variants.find("default");
            return selected == context.settings.architecture_variants.end()
                       ? context.settings.architecture
                       : selected->second;
        }
        if (name.rfind("kez.arch.", 0) == 0) {
            const std::string variant = name.substr(9);
            const auto selected       = context.settings.architecture_variants.find(variant);
            return selected == context.settings.architecture_variants.end()
                       ? context.settings.architecture
                       : selected->second;
        }
        if (name.find('.') == std::string::npos) {
            return "${" + name + "}";
        }

        const std::size_t separator             = name.find('.');
        std::string package_name                = name.substr(0, separator);
        const std::string template_package_name = package_name;
        const std::string property_name         = name.substr(separator + 1);

        if (package_name == "compiler") {
            const auto [compiler_name, compiler_version] = current_compiler(context);
            if (property_name == "prefix") {
                return parser_package_prefix(compiler_name, context);
            }
            PackageConfigPtr compiler = get_db_config(
                compiler_name, compiler_version == "system" ? "latest" : compiler_version);
            if (find_property(*compiler, property_name) == nullptr) {
                if (property_name == "incflags" && find_property(*compiler, "include") != nullptr) {
                    return format_include_path(
                        resolve_parser_scalar("${compiler.include}", context));
                }
                if ((property_name == "ldflags" || property_name == "nvldflags") &&
                    find_property(*compiler, "lib") != nullptr) {
                    const std::string path = resolve_parser_scalar("${compiler.lib}", context);
                    return property_name == "nvldflags" ? format_nvidia_library_path(path)
                                                        : format_library_path(path, context);
                }
            }
            return resolve_declared_property(name, *compiler, property_name, context);
        }

        const auto abstract = context.abstract_packages.find(package_name);
        if (abstract != context.abstract_packages.end()) {
            if (property_name.rfind("use-", 0) == 0) {
                return property_name.substr(4) == abstract->second ? "true" : "false";
            }
            package_name = abstract->second;
        }

        if (property_name.rfind("config.", 0) == 0) {
            const PackageConfigPtr config = parser_package_config(context, package_name);
            const std::string key         = config->name + ".config." + property_name.substr(7);
            const auto state              = context.named_option_values.find(key);
            if (state == context.named_option_values.end()) {
                user_config_error("template references unresolved option '" + key + "'");
            }
            if (get_selected_option_value(state->second).empty()) {
                user_config_error("template option '" + key + "' has no value");
            }
            return resolve_parser_scalar(get_selected_option_value(state->second), context);
        }
        if (property_name.rfind("env.", 0) == 0) {
            const PackageConfigPtr config = parser_package_config(context, package_name);
            const std::string key         = config->name + ".env." + property_name.substr(4);
            const auto value              = context.named_environment_values.find(key);
            if (value == context.named_environment_values.end()) {
                user_config_error("template references unresolved environment variable '" + key +
                                  "'");
            }
            return resolve_parser_scalar(value->second, context);
        }
        if (property_name == "prefix") {
            return parser_package_prefix(package_name, context);
        }
        if (property_name == "version") {
            return parser_package_version(package_name, context);
        }

        const PackageConfigPtr config = parser_package_config(context, package_name);
        if (property_name == "incflags") {
            return format_include_path(
                resolve_parser_scalar("${" + template_package_name + ".include}", context));
        }
        if (property_name == "ldflags" || property_name == "nvldflags") {
            const std::string path =
                resolve_parser_scalar("${" + template_package_name + ".lib}", context);
            return property_name == "nvldflags" ? format_nvidia_library_path(path)
                                                : format_library_path(path, context);
        }

        return resolve_declared_property(name, *config, property_name, context);
    }

    /** @brief Applies package-level overrides that target the given template name. */
    std::string apply_overrides(const std::string& template_name, std::string value,
                                UserConfigParserContext& context) {
        for (const ParsedUserPackage& package : context.packages) {
            for (const Override& override_value : package.database_config->overrides) {
                if (strip_template(override_value.target) != template_name) {
                    continue;
                }
                if (override_value.condition.has_value() &&
                    !evaluate_parser_condition(*override_value.condition, context)) {
                    continue;
                }
                if (override_value.action == ValueAction::Set) {
                    value = override_value.value;
                } else if (override_value.action == ValueAction::Append) {
                    value += (value.empty() ? "" : " ") + override_value.value;
                } else {
                    value = override_value.value + (value.empty() ? "" : " " + value);
                }
            }
        }
        return value;
    }

    /** @brief Resolves a template name to its final value by calling resolve_parser_template_base and applying overrides. */
    std::string resolve_parser_template(const std::string& name, UserConfigParserContext& context) {
        std::string value = resolve_parser_template_base(name, context);
        if (value == "${" + name + "}") {
            return value;
        }
        return apply_overrides(name, std::move(value), context);
    }
}  // namespace
/** @brief Recursively resolves all ${...} template references within a string value. */
std::string resolve_parser_scalar(const std::string& value, UserConfigParserContext& context) {
    std::string result   = value;
    std::size_t position = 0;
    while ((position = result.find("${", position)) != std::string::npos) {
        const std::size_t closing = result.find('}', position + 2);
        if (closing == std::string::npos) {
            user_config_error("unclosed template in value '" + result + "'");
        }
        const std::string name        = result.substr(position + 2, closing - position - 2);
        const std::string replacement = resolve_parser_template(name, context);
        if (replacement == "${" + name + "}") {
            position = closing + 1;
            continue;
        }
        result.replace(position, closing - position + 1, replacement);
        position += replacement.size();
    }
    return result;
}

/** @brief Resolves the installation prefix for a package based on its type and configuration. */
std::string parser_package_prefix(const std::string& package_name,
                                  UserConfigParserContext& context) {
    const auto abstract = context.abstract_packages.find(package_name);
    if (abstract != context.abstract_packages.end()) {
        return parser_package_prefix(abstract->second, context);
    }

    const std::string requested_name = canonical_package_name(context, package_name);
    const PackageConfigPtr config    = parser_package_config(context, requested_name);
    if (config->type == PackageType::System) {
        return context.settings.system_prefix.string();
    }
    if (config->type == PackageType::External) {
        const auto external = context.settings.external_packages.find(requested_name);
        if (external == context.settings.external_packages.end() ||
            external->second.prefix.empty()) {
            user_config_error("external package '" + requested_name + "' has no configured prefix");
        }
        return external->second.prefix;
    }
    if (config->type == PackageType::Vendor) {
        const Property* prefix = find_property(*config, "prefix");
        if (prefix != nullptr) {
            return resolve_declared_property(config->name + ".prefix", *config, "prefix", context);
        }
        return (context.settings.vendors_prefix /
                (requested_name + "-" + parser_package_version(requested_name, context)))
            .string();
    }
    if (config->type == PackageType::Compiler) {
        const std::string version = parser_package_version(requested_name, context);
        return version == "system" ? context.settings.system_prefix.string()
                                   : (context.settings.compilers_prefix /
                                      (requested_name + "-" + version) / requested_name)
                                         .string();
    }
    if (config->type == PackageType::Mpi) {
        const std::string version = parser_package_version(requested_name, context);
        if (version == "system") {
            return context.settings.system_prefix.string();
        }
        YAML::Node user_package = parser_user_package(context, requested_name);
        std::string compiler    = "system";
        if (yaml_has(user_package, "compiler")) {
            compiler = yaml_scalar(user_package["compiler"], "MPI compiler");
            std::replace(compiler.begin(), compiler.end(), '@', '-');
        }
        return (context.settings.mpis_prefix / (requested_name + "-" + version + "-" + compiler) /
                requested_name)
            .string();
    }
    return (context.settings.install_prefix / requested_name).string();
}
