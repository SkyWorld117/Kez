#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <database/config.hpp>
#include <dependency_resolver/requirements.hpp>
#include <filesystem>
#include <parser/parser_internal.hpp>
#include <string>
#include <utility>
#include <utils/colored_io.hpp>
#include <utils/string_utils.hpp>
#include <utils/yaml_utils.hpp>
#include <variant>
#include <vector>

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

    /** @brief Retrieves the PackageConfig for a given package name, consulting indices, caches, and the database. */
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

    /** @brief Tokenizes a boolean condition expression into tokens for parsing. */
    std::vector<std::string> tokenize_condition(const std::string& expression) {
        std::vector<std::string> tokens;
        std::string current;
        auto flush = [&] {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        };

        for (std::size_t i = 0; i < expression.size(); ++i) {
            const char character = expression[i];
            if (std::isspace(static_cast<unsigned char>(character))) {
                flush();
            } else if (character == '(' || character == ')') {
                flush();
                tokens.emplace_back(1, character);
            } else if (character == '&' || character == '|') {
                flush();
                if (i + 1 >= expression.size() || expression[i + 1] != character) {
                    user_config_error("condition contains an incomplete logical operator: " +
                                      expression);
                }
                tokens.emplace_back(2, character);
                ++i;
            } else {
                current += character;
            }
        }
        flush();
        return tokens;
    }

    /** @brief Removes the "${" and "}" delimiters from a template string if present. */
    std::string strip_template(const std::string& value) {
        if (value.size() >= 3 && value.rfind("${", 0) == 0 && value.back() == '}') {
            return value.substr(2, value.size() - 3);
        }
        return value;
    }

    /** @brief Checks whether an option name corresponds to a declared abstract-package selector. */
    bool is_declared_abstract_selector(const std::string& option_name,
                                       UserConfigParserContext& context) {
        const std::size_t separator = option_name.find('.');
        if (separator == std::string::npos ||
            option_name.find('.', separator + 1) != std::string::npos) {
            return false;
        }

        const std::string selector = option_name.substr(separator + 1);
        if (selector.rfind("use-", 0) != 0 || selector.size() == 4) {
            return false;
        }

        const std::string package_name   = option_name.substr(0, separator);
        const std::string implementation = selector.substr(4);
        const PackageConfigPtr config    = parser_package_config(context, package_name);
        return config->type == PackageType::Abstract &&
               std::find(config->implementations.begin(), config->implementations.end(),
                         implementation) != config->implementations.end();
    }

    /** @brief Evaluates a version comparison expression against the resolved version of a template. */
    bool compare_version_expression(const std::string& expression,
                                    UserConfigParserContext& context) {
        if (expression.rfind("${", 0) != 0) {
            user_config_error("version condition must start with a template: " + expression);
        }
        const std::size_t closing = expression.find('}');
        if (closing == std::string::npos) {
            user_config_error("version condition contains an unclosed template: " + expression);
        }

        const std::string current =
            resolve_parser_scalar(expression.substr(0, closing + 1), context);
        const std::string comparisons = expression.substr(closing + 1);
        std::size_t begin             = 0;
        while (begin <= comparisons.size()) {
            const std::size_t end        = comparisons.find(',', begin);
            const std::string comparison = comparisons.substr(begin, end - begin);
            static const std::vector<std::string> operators = {">=", "<=", "==", "!=", ">", "<"};
            std::string operation;
            for (const std::string& candidate : operators) {
                if (comparison.rfind(candidate, 0) == 0) {
                    operation = candidate;
                    break;
                }
            }
            if (operation.empty() || comparison.size() == operation.size()) {
                user_config_error("invalid version comparison: " + comparison);
            }

            const int result = compare_versions(current, comparison.substr(operation.size()));
            if ((operation == ">" && result <= 0) || (operation == ">=" && result < 0) ||
                (operation == "<" && result >= 0) || (operation == "<=" && result > 0) ||
                (operation == "==" && result != 0) || (operation == "!=" && result == 0)) {
                return false;
            }
            if (end == std::string::npos) {
                break;
            }
            begin = end + 1;
        }
        return true;
    }

    struct ConditionCursor {
        const std::vector<std::string>& tokens;  ///< Tokenised condition.
        UserConfigParserContext& context;        ///< Parser context for evaluating operands.
        std::size_t position = 0;                ///< Current token index in @p tokens.
    };

    bool parse_condition_or(ConditionCursor& cursor);

    /** @brief Parses a primary condition: a parenthesized sub-expression, a literal, or a condition keyword. */
    bool parse_condition_primary(ConditionCursor& cursor) {
        if (cursor.position >= cursor.tokens.size()) {
            user_config_error("condition ended before an operand was provided");
        }
        const std::string token = cursor.tokens[cursor.position++];
        if (token == "(") {
            const bool result = parse_condition_or(cursor);
            if (cursor.position >= cursor.tokens.size() || cursor.tokens[cursor.position] != ")") {
                user_config_error("condition contains unmatched parentheses");
            }
            ++cursor.position;
            return result;
        }
        if (token == "true" || token == "false") {
            return token == "true";
        }
        if (token == "required") {
            if (cursor.position >= cursor.tokens.size()) {
                user_config_error("required condition is missing a package name");
            }
            return requirements_satisfied({cursor.tokens[cursor.position++]},
                                          cursor.context.dependencies,
                                          cursor.context.abstract_packages);
        }
        if (token == "environment") {
            if (cursor.position >= cursor.tokens.size()) {
                user_config_error("environment condition is missing a variable name");
            }
            const std::string name = strip_template(cursor.tokens[cursor.position++]);
            const auto parsed      = cursor.context.named_environment_values.find(name);
            if (parsed != cursor.context.named_environment_values.end()) {
                return !parsed->second.empty();
            }
            const char* value = std::getenv(name.c_str());
            return value != nullptr && *value != '\0';
        }
        if (token == "version") {
            if (cursor.position >= cursor.tokens.size()) {
                user_config_error("version condition is missing a comparison");
            }
            return compare_version_expression(cursor.tokens[cursor.position++], cursor.context);
        }

        if (cursor.position >= cursor.tokens.size()) {
            user_config_error("option condition is missing an enabled state: " + token);
        }
        const std::string option_name    = strip_template(token);
        const std::string expected_state = cursor.tokens[cursor.position++];
        const auto option                = cursor.context.named_option_values.find(option_name);
        if (option == cursor.context.named_option_values.end()) {
            if (is_declared_abstract_selector(option_name, cursor.context)) {
                return false;
            }
            user_config_error("condition references unresolved option '" + option_name + "'");
        }

        const bool expected_enabled  = expected_state == "true" || expected_state == "enabled";
        const bool expected_disabled = expected_state == "false" || expected_state == "disabled";
        if (!expected_enabled && !expected_disabled) {
            user_config_error("condition has invalid option state '" + expected_state + "'");
        }
        bool result = option->second.enabled == expected_enabled;
        if (cursor.position < cursor.tokens.size() && cursor.tokens[cursor.position] != "&&" &&
            cursor.tokens[cursor.position] != "||" && cursor.tokens[cursor.position] != ")") {
            result = result &&
                     get_selected_option_value(option->second) == cursor.tokens[cursor.position];
            ++cursor.position;
        }
        return result;
    }

    /** @brief Parses a unary condition, optionally preceded by the "not" operator. */
    bool parse_condition_unary(ConditionCursor& cursor) {
        if (cursor.position < cursor.tokens.size() && cursor.tokens[cursor.position] == "not") {
            ++cursor.position;
            return !parse_condition_unary(cursor);
        }
        return parse_condition_primary(cursor);
    }

    /** @brief Parses a conjunction of unary conditions separated by "&&". */
    bool parse_condition_and(ConditionCursor& cursor) {
        bool result = parse_condition_unary(cursor);
        while (cursor.position < cursor.tokens.size() && cursor.tokens[cursor.position] == "&&") {
            ++cursor.position;
            const bool right = parse_condition_unary(cursor);
            result           = result && right;
        }
        return result;
    }

    /** @brief Parses a disjunction of AND-conditions separated by "||". */
    bool parse_condition_or(ConditionCursor& cursor) {
        bool result = parse_condition_and(cursor);
        while (cursor.position < cursor.tokens.size() && cursor.tokens[cursor.position] == "||") {
            ++cursor.position;
            const bool right = parse_condition_and(cursor);
            result           = result || right;
        }
        return result;
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

    /** @brief Retrieves the PackageConfig for a property lookup, resolving "compiler" to the active compiler and abstract packages to their concrete implementation. */
    PackageConfigPtr property_package_config(const std::string& package_name,
                                             UserConfigParserContext& context) {
        if (package_name == "compiler") {
            const auto [compiler_name, compiler_version] = current_compiler(context);
            return get_db_config(compiler_name,
                                 compiler_version == "system" ? "latest" : compiler_version);
        }
        const auto abstract = context.abstract_packages.find(package_name);
        return parser_package_config(
            context, abstract == context.abstract_packages.end() ? package_name : abstract->second);
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

    /** @brief Tokenizes and evaluates a boolean condition expression. */
    bool evaluate_parser_condition(const std::string& expression,
                                   UserConfigParserContext& context) {
        const std::vector<std::string> tokens = tokenize_condition(expression);
        if (tokens.empty()) {
            user_config_error("condition must not be empty");
        }
        ConditionCursor cursor {tokens, context};
        const bool result = parse_condition_or(cursor);
        if (cursor.position != tokens.size()) {
            user_config_error("condition contains unexpected token '" + tokens[cursor.position] +
                              "'");
        }
        return result;
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
                return compiler_version == "system" ? context.settings.system_prefix.string()
                                                    : (context.settings.compilers_prefix /
                                                       (compiler_name + "-" + compiler_version))
                                                          .string();
            }
            PackageConfigPtr compiler = get_db_config(
                compiler_name, compiler_version == "system" ? "latest" : compiler_version);
            if (find_property(*compiler, property_name) == nullptr) {
                if (property_name == "includes" && find_property(*compiler, "include") != nullptr) {
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
        if (find_property(*config, property_name) == nullptr) {
            if (property_name == "includes" && find_property(*config, "include") != nullptr) {
                return format_include_path(
                    resolve_parser_scalar("${" + template_package_name + ".include}", context));
            }
            if ((property_name == "ldflags" || property_name == "nvldflags") &&
                find_property(*config, "lib") != nullptr) {
                const std::string path =
                    resolve_parser_scalar("${" + template_package_name + ".lib}", context);
                return property_name == "nvldflags" ? format_nvidia_library_path(path)
                                                    : format_library_path(path, context);
            }
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

/** @brief Applies conditional overrides to a string value based on evaluated parser conditions. */
std::string apply_parser_conditions(const ConfigurableValue<std::string>& configurable,
                                    const std::string& base_value,
                                    UserConfigParserContext& context) {
    std::string result = base_value;
    for (const ConditionalValue<std::string>& condition : configurable.conditions) {
        if (!evaluate_parser_condition(condition.condition, context)) {
            continue;
        }
        if (condition.action == ValueAction::Set) {
            result = condition.value;
            break;
        }
        if (condition.action == ValueAction::Append) {
            result += (result.empty() ? "" : " ") + condition.value;
        } else {
            result = condition.value + (result.empty() ? "" : " " + result);
        }
    }
    return result;
}

/** @brief Applies conditional overrides to a boolean value based on evaluated parser conditions. */
bool apply_parser_conditions(const ConfigurableValue<bool>& configurable, bool base_value,
                             UserConfigParserContext& context) {
    bool result = base_value;
    for (const ConditionalValue<bool>& condition : configurable.conditions) {
        if (evaluate_parser_condition(condition.condition, context)) {
            result = condition.value;
            break;
        }
    }
    return result;
}

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

/** @brief Checks whether a package has a named property, accounting for abstract-to-concrete resolution. */
bool parser_package_has_property(const std::string& package_name, const std::string& property_name,
                                 UserConfigParserContext& context) {
    const PackageConfigPtr config = property_package_config(package_name, context);
    return has_property(*config, property_name);
}
