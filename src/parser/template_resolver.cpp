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

/**
 * @brief Print a fatal user-configuration error and terminate the process.
 *
 * Formats and prints an error message via the ERROR() macro, then immediately
 * exits with EXIT_FAILURE. This is the standard way to report invalid or
 * unresolvable user configuration for all parser components.
 *
 * @param message  A human-readable description of what is wrong with the
 *                 user's configuration.
 *
 * @warning This function never returns (annotated [[noreturn]]).
 */
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

    /**
     * @brief Retrieve the parsed database configuration for a package.
     *
     * First checks whether the package is already in the explicit install list
     * (@p context.packages). If not, looks in the extra-config cache
     * (@p context.extra_configs); on a cache miss, calls @ref get_db_config()
     * to parse and cache it. When the database config's @p name differs from
     * the requested name (e.g. due to canonicalisation), an alias mapping is
     * recorded in @p context.package_aliases.
     *
     * @param context      Parser context that caches loaded configurations.
     * @param package_name The package name (or alias) to look up.
     * @return A shared pointer to the immutable @ref PackageConfig.
     *
     * @note Terminates via user_config_error() if the package is not found in
     *       the database (propagated from @ref get_db_config()).
     */
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

    /**
     * @brief Retrieve the raw user-configuration YAML node for a package.
     *
     * Looks up @p package_name (after alias resolution) in the parsed package
     * index. Returns the user's original YAML fragment if the package is part
     * of the explicit install list, or a null YAML::Node otherwise.
     *
     * @param context      Parser context with parsed package index.
     * @param package_name The package name (or alias) to look up.
     * @return The user's YAML configuration for the package, or
     *         a default-constructed (null) YAML::Node if not found.
     */
    YAML::Node parser_user_package(UserConfigParserContext& context,
                                   const std::string& package_name) {
        const std::string requested_name = canonical_package_name(context, package_name);
        const auto parsed                = context.package_indices.find(requested_name);
        if (parsed == context.package_indices.end()) {
            return YAML::Node();
        }
        return context.packages[parsed->second].user_config;
    }

    /**
     * @brief Tokenise a boolean condition expression into tokens.
     *
     * Splits @p expression into tokens separated by whitespace. Parentheses
     * become single-character tokens; the logical operators "&&" and "||" are
     * recognised as two-character tokens. All other contiguous non-whitespace
     * sequences are returned as single tokens (operands). Incomplete
     * single-character '&' or '|' operators cause a fatal error.
     *
     * @param expression  A condition expression string (e.g.
     *                    `"required zlib && (version ${x} >=1.0)"`).
     * @return A vector of token strings that can be parsed by the recursive-
     *         descent parser.
     *
     * @warning Calls user_config_error() and terminates if a lone '&' or '|'
     *          appears without its matching partner.
     */
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

    /**
     * @brief Strip the "${}" template delimiters from a string, if present.
     *
     * If @p value starts with "${" and ends with "}", the outer delimiters are
     * removed. Otherwise @p value is returned unchanged.
     *
     * @param value  A string that may be a template reference like
     *               `${package_name}`.
     * @return The inner content when delimiters are present, or @p value
     *         unchanged.
     */
    std::string strip_template(const std::string& value) {
        if (value.size() >= 3 && value.rfind("${", 0) == 0 && value.back() == '}') {
            return value.substr(2, value.size() - 3);
        }
        return value;
    }

    /**
     * @brief Check whether an option name refers to a declared abstract-package
     *        selector.
     *
     * An abstract-package selector has the form `<package>.use-<implementation>`:
     * - The name contains exactly one '.'.
     * - The part after the '.' starts with "use-" and is longer than 4 chars.
     * - The package before the '.' is an @ref PackageType::Abstract package.
     * - The implementation name after "use-" appears in the package's
     *   @ref PackageConfig::implementations list.
     *
     * @param option_name  The fully-qualified option name to check.
     * @param context      Parser context for loading package configurations.
     * @return @c true if @p option_name is a valid abstract selector;
     *         @c false otherwise.
     *
     * @note Terminates via user_config_error() if the package config cannot be
     *       loaded (propagated from @ref parser_package_config()).
     */
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

    /**
     * @brief Evaluate a version-comparison expression.
     *
     * Parses an expression of the form `${template} op1 value1,op2 value2,...`
     * where each comma-separated comparison is evaluated against the resolved
     * template value. All comparisons must pass for the expression to return
     * true (implicit AND). Supported operators: >=, <=, ==, !=, >, <.
     *
     * @param expression  The version condition string (must start with a
     *                    template reference).
     * @param context     Parser context for template resolution.
     * @return @c true if all individual version comparisons succeed;
     *         @c false if any comparison fails.
     *
     * @warning Terminates with user_config_error() if:
     *         - The expression does not start with "${".
     *         - The template is unclosed.
     *         - A comparison segment lacks an operator or an operand.
     */
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

    /**
     * @brief Cursor state for the recursive-descent condition parser.
     *
     * Tracks the current position within the tokenised condition expression
     * during evaluation. All parsing functions receive and advance this cursor.
     */
    struct ConditionCursor {
        const std::vector<std::string>& tokens;  ///< Tokenised condition.
        UserConfigParserContext& context;        ///< Parser context for evaluating operands.
        std::size_t position = 0;                ///< Current token index in @p tokens.
    };

    bool parse_condition_or(ConditionCursor& cursor);

    /**
     * @brief Parse and evaluate a primary condition operand.
     *
     * A primary operand is one of:
     * - A parenthesised sub-expression `(...)`.
     * - A boolean literal `true` or `false`.
     * - A `required` predicate: checks whether a package is in the dependency set.
     * - An `environment` predicate: checks whether an env-var is non-empty.
     * - A `version` predicate: delegates to @ref compare_version_expression().
     * - An option-state condition: checks whether a named option is enabled or
     *   disabled, optionally also verifying its selected value.
     *
     * @param cursor  The condition cursor, advanced past the consumed tokens.
     * @return The boolean result of evaluating the primary operand.
     *
     * @warning Terminates with user_config_error() on:
     *         - Unexpected end of tokens before an operand.
     *         - Unmatched parentheses.
     *         - Missing package name after `required`.
     *         - Missing variable name after `environment`.
     *         - Missing comparison after `version`.
     *         - References to an unresolved option.
     *         - Invalid option state string.
     */
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

    /**
     * @brief Parse and evaluate a unary `not` expression.
     *
     * If the current token is `not`, the next sub-expression is negated.
     * Otherwise the expression is delegated to @ref parse_condition_primary().
     *
     * @param cursor  The condition cursor, advanced past consumed tokens.
     * @return The boolean result of the (possibly negated) sub-expression.
     *
     * @note `not` can be chained (e.g. `not not true`).
     */
    bool parse_condition_unary(ConditionCursor& cursor) {
        if (cursor.position < cursor.tokens.size() && cursor.tokens[cursor.position] == "not") {
            ++cursor.position;
            return !parse_condition_unary(cursor);
        }
        return parse_condition_primary(cursor);
    }

    /**
     * @brief Parse and evaluate an `&&` chain (conjunction).
     *
     * Evaluates left-to-right with short-circuit semantics:
     * as soon as any operand evaluates to @c false, the remainder is skipped.
     *
     * @param cursor  The condition cursor, advanced past consumed tokens.
     * @return The boolean result of the conjunction.
     */
    bool parse_condition_and(ConditionCursor& cursor) {
        bool result = parse_condition_unary(cursor);
        while (cursor.position < cursor.tokens.size() && cursor.tokens[cursor.position] == "&&") {
            ++cursor.position;
            const bool right = parse_condition_unary(cursor);
            result           = result && right;
        }
        return result;
    }

    /**
     * @brief Parse and evaluate a `||` chain (disjunction), top-level rule.
     *
     * Entry point for the recursive-descent condition parser. Evaluates
     * left-to-right with short-circuit semantics: as soon as any operand
     * evaluates to @c true, the remainder is skipped.
     *
     * Grammar (in order of precedence):
     * ```
     * or_expr  ::= and_expr ('||' and_expr)*
     * and_expr ::= unary_expr ('&&' unary_expr)*
     * unary_expr ::= 'not' unary_expr | primary_expr
     * primary_expr ::= '(' or_expr ')'
     *                | 'true' | 'false'
     *                | 'required' <package>
     *                | 'environment' <variable>
     *                | 'version' <comparison>
     *                | <option> <state> [<value>]
     * ```
     *
     * @param cursor  The condition cursor, advanced past all consumed tokens.
     * @return The boolean result of the full expression.
     */
    bool parse_condition_or(ConditionCursor& cursor) {
        bool result = parse_condition_and(cursor);
        while (cursor.position < cursor.tokens.size() && cursor.tokens[cursor.position] == "||") {
            ++cursor.position;
            const bool right = parse_condition_and(cursor);
            result           = result || right;
        }
        return result;
    }

    /**
     * @brief Determine the currently active compiler for a package.
     *
     * Reads the `compiler` field from the current package's user configuration.
     * If absent, defaults to "system" (which maps to gcc@system). A valid
     * specification is either the literal "system" or the form `<name>@<version>`.
     *
     * @param context  Parser context providing access to user config and the
     *                 current package name.
     * @return A pair `{compiler_name, compiler_version}`.
     *         For "system" this is `{"gcc", "system"}`.
     *
     * @warning Terminates with user_config_error() if the specification is
     *          neither "system" nor a valid `<name>@<version>`.
     */
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

    /**
     * @brief Check whether a compiler name identifies an NVIDIA compiler.
     *
     * @param compiler  The compiler name string (e.g. "gcc", "nvhpc", "nvcc").
     * @return @c true if @p compiler contains "nvhpc" or equals "nvc" or
     *         "nvcc"; @c false otherwise.
     */
    bool is_nvidia_compiler(const std::string& compiler) {
        return compiler.find("nvhpc") != std::string::npos || compiler == "nvc" ||
               compiler == "nvcc";
    }

    /**
     * @brief Format a path as a GCC/Clang-style `-I` include flag.
     *
     * @param path  The include directory path.
     * @return The string `"-I<path>"`.
     */
    std::string format_include_path(const std::string& path) { return "-I" + path; }

    /**
     * @brief Format a library path for the NVIDIA linker (nvlink wrapper).
     *
     * Produces a `-L` flag for the search path and a `-Xlinker -rpath,<path>`
     * for the runtime library search path, as required by NVIDIA's compilation
     * toolchain.
     *
     * @param path  The library directory path.
     * @return The string `"-L<path> -Xlinker -rpath,<path>"`.
     */
    std::string format_nvidia_library_path(const std::string& path) {
        return "-L" + path + " -Xlinker -rpath," + path;
    }

    /**
     * @brief Format a library path as linker flags, dispatching on the current
     *        compiler.
     *
     * For NVIDIA compilers, delegates to @ref format_nvidia_library_path().
     * For all other compilers (GCC, Clang, etc.), produces a `-L` flag and a
     * `-Wl,-rpath,<path>` runtime search path flag.
     *
     * @param path    The library directory path.
     * @param context Parser context used to determine the active compiler.
     * @return A string of linker flags for the library path.
     */
    std::string format_library_path(const std::string& path, UserConfigParserContext& context) {
        return is_nvidia_compiler(current_compiler(context).first)
                   ? format_nvidia_library_path(path)
                   : "-L" + path + " -Wl,-rpath," + path;
    }

    /**
     * @brief Retrieve the database configuration for a package referenced as a
     *        property source.
     *
     * Special handling for the pseudo-package "compiler": resolves the current
     * compiler's name and version, and loads its database config. For all other
     * packages, resolves abstract-to-concrete mappings via
     * @p context.abstract_packages before delegating to
     * @ref parser_package_config().
     *
     * @param package_name  The property-source package name ("compiler" or any
     *                      valid package name).
     * @param context       Parser context for loading configurations and
     *                      resolving abstract packages.
     * @return A shared pointer to the package's @ref PackageConfig.
     *
     * @note Terminates via user_config_error() if the package cannot be found
     *       (propagated).
     */
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

    /**
     * @brief Resolve a @ref ConfigurableValue<std::string> to its final string.
     *
     * Takes the optional default value and applies all conditional overrides
     * whose conditions evaluate to true in the current context.
     *
     * @param configurable  The configurable value specification with conditions.
     * @param context       Parser context for condition evaluation.
     * @return The resolved string. If no default is present and no condition
     *         matches, returns an empty string.
     */
    std::string apply_string_value(const ConfigurableValue<std::string>& configurable,
                                   UserConfigParserContext& context) {
        const std::string base = configurable.default_value.value_or("");
        return apply_parser_conditions(configurable, base, context);
    }

    /**
     * @brief Resolve a named property from a package's database configuration.
     *
     * Looks up @p property_name in the given @p config. If the property stores
     * a plain string, it is returned directly. If it stores a
     * @ref ConfigurableValue<std::string>, conditions are evaluated via
     * @ref apply_string_value(). The resolved value is then scanned for nested
     * template references via @ref resolve_parser_scalar().
     *
     * Cyclic template references are detected and rejected: the function
     * inserts @p template_name into @p context.resolving_templates before
     * resolving and removes it afterwards. If the name is already present, a
     * fatal error is raised.
     *
     * @param template_name  The fully-qualified template name being resolved
     *                       (e.g. `"zlib.lib"`), used for cycle detection.
     * @param config         The database configuration containing the property.
     * @param property_name  The name of the property to resolve.
     * @param context        Parser context for condition evaluation and
     *                       nested template resolution.
     * @return The fully-resolved property value as a string.
     *
     * @warning Terminates with user_config_error() if:
     *         - The property does not exist on the package.
     *         - A cyclic template reference is detected.
     */
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

    /**
     * @brief Evaluate a condition expression string to a boolean.
     *
     * Tokenises the expression via @ref tokenize_condition() and evaluates it
     * via the recursive-descent parser (@ref parse_condition_or()). Asserts
     * that all tokens are consumed after parsing.
     *
     * @param expression  The condition expression string.
     * @param context     Parser context for evaluating operands (option states,
     *                    environment values, dependencies, etc.).
     * @return @c true if the expression evaluates to true, @c false otherwise.
     *
     * @warning Terminates with user_config_error() if:
     *         - The expression is empty.
     *         - Unconsumed tokens remain after parsing (syntax error).
     *         - Any parse error is encountered (propagated from sub-parsers).
     */
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

    /**
     * @brief Determine the version string for a package in the current context.
     *
     * Version resolution follows a priority chain:
     * 1. If the user explicitly specified a `version` in their YAML config,
     *    that version is used (with any `@local` suffix stripped).
     * 2. For @ref PackageType::External packages, the version is read from
     *    @p context.settings.external_packages (terminates on missing version).
     * 3. For @ref PackageType::System packages, the version is read from the
     *    `state.yaml` file under @p context.settings.system_prefix (terminates
     *    on missing file or missing entry).
     * 4. If the package is the active compiler itself, the compiler version
     *    (from @ref current_compiler()) is returned.
     * 5. If the package has source releases, the first release's version is
     *    returned.
     * 6. Otherwise terminates with a fatal error.
     *
     * @param package_name  The package name (or alias) whose version is needed.
     * @param context       Parser context with user config, settings, and
     *                      abstract-package mappings.
     * @return The resolved version string.
     *
     * @warning Terminates with user_config_error() in all failure cases
     *          (missing version, missing state file, missing state entry).
     */
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

    /**
     * @brief Resolve a single template name to its base value (before applying
     *        overrides).
     *
     * Handles the following template categories:
     * - **Built-in keywords**: `source` (source directory), `kez.prefix`
     *   (install prefix), `kez.arch` / `kez.arch.<variant>` (architecture).
     * - **Unrecognised single-segment names**: returned as the literal
     *   `${name}` for the caller to treat as unresolved.
     * - **Two-segment names** `<package>.<property>`:
     *   - `compiler.prefix`: resolved to the compiler's install prefix.
     *   - `compiler.<property>`: resolved from the compiler's database config.
     *   - Abstract package properties: concrete implementation is substituted.
     *   - `<pkg>.config.<key>`: a user-configured option value.
     *   - `<pkg>.env.<key>`: a user-configured environment variable.
     *   - `<pkg>.prefix`: delegates to @ref parser_package_prefix().
     *   - `<pkg>.version`: delegates to @ref parser_package_version().
     *   - Other properties: resolved from the package's database config,
     *     with automatic aliasing (`includes` -> `include`,
     *     `ldflags`/`nvldflags` -> `lib`).
     *
     * @param name    The template name to resolve (e.g. `"zlib.prefix"`,
     *                `"kez.arch"`).
     * @param context Parser context with settings, user config, and cached
     *                database configurations.
     * @return The resolved value as a string, or the literal
     *         `"${<name>}"` if the name is unrecognised.
     *
     * @warning Terminates with user_config_error() when:
     *         - An unrecognised property is accessed on a package.
     *         - An option or environment reference cannot be resolved.
     *         - A cyclic template reference is detected (propagated).
     */
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

    /**
     * @brief Apply all matching overrides from every parsed package to a
     *        resolved template value.
     *
     * Iterates over every package's @ref Override list. For each override
     * whose stripped target matches @p template_name and whose optional
     * condition evaluates to true, the override's action (Set, Append, or
     * Prepend) is applied to @p value.
     *
     * @param template_name  The fully-qualified template name to match against
     *                       override targets (e.g. `"zil.lib"`).
     * @param value          The current value of the template before overrides.
     * @param context        Parser context for evaluating override conditions.
     * @return The value after all matching overrides have been applied.
     */
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

    /**
     * @brief Resolve a single template name to its final value, including
     *        overrides.
     *
     * Calls @ref resolve_parser_template_base() to obtain the base value, then
     * applies any matching overrides via @ref apply_overrides(). If the base
     * value is the literal `"${<name>}"` (indicating an unrecognised
     * template), it is returned unchanged without applying overrides.
     *
     * @param name    The template name to resolve.
     * @param context Parser context for resolution and override evaluation.
     * @return The final resolved value, or `"${<name>}"` if the template is
     *         not recognised.
     */
    std::string resolve_parser_template(const std::string& name, UserConfigParserContext& context) {
        std::string value = resolve_parser_template_base(name, context);
        if (value == "${" + name + "}") {
            return value;
        }
        return apply_overrides(name, std::move(value), context);
    }
}  // namespace

/**
 * @brief Evaluate conditional overrides for a configurable string value and
 *        return the resulting value.
 *
 * Walks the list of @ref ConditionalValue entries attached to a
 * @ref ConfigurableValue<std::string>. For each entry whose condition
 * evaluates to true in the current @p context, the corresponding action
 * (Set / Append / Prepend) is applied to @p base_value. The first matching
 * entry with action Set replaces @p base_value entirely; Append and Prepend
 * modify it in place. If no condition matches, @p base_value is returned
 * unchanged.
 *
 * @param configurable  The configurable value whose conditions are evaluated.
 * @param base_value    The starting value before any conditional overrides.
 * @param context       The parser context used to evaluate conditions.
 * @return The final string after applying all matching conditions.
 */
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

/**
 * @brief Evaluate conditional overrides for a configurable boolean value.
 *
 * Similar to the string overload, but operates on a
 * @ref ConfigurableValue<bool>. Each matching condition's action is applied
 * to the boolean base value. Note that Append and Prepend behave identically
 * to Set for boolean values (the boolean is simply replaced). Evaluation
 * stops at the first matching condition (regardless of action).
 *
 * @param configurable  The configurable value whose conditions are evaluated.
 * @param base_value    The starting boolean before any conditional overrides.
 * @param context       The parser context used to evaluate conditions.
 * @return The final boolean after applying all matching conditions.
 */
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

/**
 * @brief Resolve all template placeholders in a scalar value string.
 *
 * Scans @p value for occurrences of `${<name>}` and replaces each with the
 * result of @ref resolve_parser_template(). Resolution is iterative: after
 * each replacement the scan continues from the end of the substituted text,
 * allowing templates whose values contain further template references to be
 * fully expanded.
 *
 * If a template name is not recognised, @ref resolve_parser_template()
 * returns the literal `${<name>}`; the scanner skips past it and continues
 * looking for other templates.
 *
 * @param value    The scalar string that may contain zero or more template
 *                 placeholders.
 * @param context  The parser context providing template resolution and
 *                 cycle detection.
 * @return The fully-resolved string with all recognised template
 *         placeholders expanded.
 *
 * @warning Terminates with user_config_error() if an unclosed `${` marker
 *          is found (missing closing `}`).
 */
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

/**
 * @brief Resolve the installation prefix for a given package.
 *
 * Returns the absolute path under which the package's build artifacts are
 * installed. The prefix depends on the package type:
 *
 * - @ref PackageType::System: returns @p context.settings.system_prefix.
 * - @ref PackageType::External: returns the user-configured prefix from
 *   @p context.settings.external_packages (terminates if missing).
 * - @ref PackageType::Vendor: returns the vendor's declared `prefix`
 *   property if present, otherwise falls back to
 *   `<vendors_prefix>/<name>-<version>`.
 * - @ref PackageType::Compiler: returns
 *   `<compilers_prefix>/<name>-<version>/<name>` for non-system compilers,
 *   or @p context.settings.system_prefix for system compilers.
 * - @ref PackageType::Mpi: returns
 *   `<mpis_prefix>/<name>-<version>-<compiler>/<name>` for non-system MPI,
 *   or @p context.settings.system_prefix for system MPI.
 * - All other types (regular packages): returns
 *   `<install_prefix>/<name>`.
 *
 * Abstract packages are resolved to their concrete implementation first.
 *
 * @param package_name  The canonical name of the package.
 * @param context       The parser context containing installation-prefix
 *                      settings and user configuration.
 * @return The absolute installation prefix path as a string.
 *
 * @warning Terminates with user_config_error() if:
 *         - An external package has no configured prefix.
 *         - A version cannot be determined (propagated from
 *           @ref parser_package_version()).
 */
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

/**
 * @brief Check whether a package declares a specific property (or an alias).
 *
 * Consults the package's database configuration for a property matching
 * @p property_name.  This function respects the property aliasing rules
 * defined by @ref has_property() (e.g. "includes" is aliased to "include",
 * "ldflags"/"nvldflags" to "lib"). Abstract packages are resolved to their
 * concrete implementation via @ref property_package_config() before the
 * lookup.
 *
 * @param package_name   The canonical name of the package to query.
 * @param property_name  The property name to look up (e.g. "include", "lib",
 *                       "includes", "ldflags").
 * @param context        The parser context that provides access to loaded
 *                       database configurations.
 * @return @c true if the package has the property (or an alias matches),
 *         @c false otherwise.
 *
 * @see has_property()  For the alias rules applied during lookup.
 * @see find_property()  For the direct property search without aliasing.
 */
bool parser_package_has_property(const std::string& package_name, const std::string& property_name,
                                 UserConfigParserContext& context) {
    const PackageConfigPtr config = property_package_config(package_name, context);
    return has_property(*config, property_name);
}
