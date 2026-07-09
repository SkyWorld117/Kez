#include <cctype>
#include <database/condition_parser.hpp>
#include <database/parser_utils.hpp>
#include <functional>
#include <vector>

namespace {
    /**
     * @brief Tokenizes a condition expression string into a sequence of tokens.
     *
     * Splits a logical condition expression (supporting parentheses, `&&`, `||`,
     * and operand tokens) into a vector of strings. Whitespace is treated as a
     * token separator. Parentheses become single-character tokens, double
     * ampersand and double pipe become two-character tokens, and all other
     * contiguous non-whitespace, non-operator characters are grouped into a
     * single operand token.
     *
     * @param expression The raw condition expression string to tokenize.
     * @param node       The YAML node from which the expression originated,
     *                   used for error reporting.
     * @param path       The YAML path to the node, used for error reporting.
     * @param context    The parser context providing error-reporting utilities.
     * @return std::vector<std::string> A vector of tokens in left-to-right
     *         order.
     *
     * @note Terminates the program via `fail_config` if a single `&` or `|`
     *       character is encountered (incomplete logical operator).
     */
    std::vector<std::string> tokenize_condition(const std::string& expression,
                                                const YAML::Node& node, const std::string& path,
                                                const DatabaseParserContext& context) {
        std::vector<std::string> tokens;
        std::string current;

        // Flushes the current accumulative token into the token list and resets
        // the accumulator. Called at every token boundary (whitespace or
        // operator character).
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
                    fail_config(node, path, "contains an incomplete logical operator", context);
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
     * @brief Validates the syntax of a version comparison token.
     *
     * A valid version comparison must start with a template variable
     * (`${...}`) immediately followed by one or more comma-separated
     * comparisons, each consisting of a comparison operator (`>=`, `<=`, `==`,
     * `!=`, `>`, `<`) and a version string. For example:
     * `${prefix}>=1.0,<2.0` is valid.
     *
     * @param token   The version comparison token to validate.
     * @param node    The YAML node from which the expression originated,
     *                used for error reporting.
     * @param path    The YAML path to the node, used for error reporting.
     * @param context The parser context providing error-reporting utilities.
     *
     * @note Terminates the program via `fail_config` on any of the following:
     *       - The token does not start with `${`.
     *       - The closing `}` is missing or empty (i.e. `${}`).
     *       - No comparison follows the template variable.
     *       - A comparison segment lacks a valid operator prefix or is
     *         operator-only (no version string).
     */
    void validate_version_condition(const std::string& token, const YAML::Node& node,
                                    const std::string& path, const DatabaseParserContext& context) {
        if (token.rfind("${", 0) != 0) {
            fail_config(node, path, "version comparison must start with a template variable",
                        context);
        }
        const std::size_t closing = token.find('}');
        if (closing == std::string::npos || closing == 2) {
            fail_config(node, path, "contains an invalid version template", context);
        }
        std::string comparisons = token.substr(closing + 1);
        if (comparisons.empty()) {
            fail_config(node, path, "contains no version comparison", context);
        }

        std::size_t begin = 0;
        while (begin <= comparisons.size()) {
            const std::size_t end        = comparisons.find(',', begin);
            const std::string comparison = comparisons.substr(begin, end - begin);
            static const std::vector<std::string> operators = {">=", "<=", "==", "!=", ">", "<"};
            bool valid                                      = false;
            for (const std::string& candidate : operators) {
                if (comparison.rfind(candidate, 0) == 0 && comparison.size() > candidate.size()) {
                    valid = true;
                    break;
                }
            }
            if (!valid) {
                fail_config(node, path, "contains invalid version comparison '" + comparison + "'",
                            context);
            }
            if (end == std::string::npos) {
                break;
            }
            begin = end + 1;
        }
    }
}  // namespace

/**
 * @brief Validates the syntax of a condition expression string.
 *
 * Expects a boolean expression composed of the following grammar:
 * @code
 *   expression  ::= or-expr
 *   or-expr     ::= and-expr ("||" and-expr)*
 *   and-expr    ::= unary ("&&" unary)*
 *   unary       ::= "not" unary | primary
 *   primary     ::= "(" expression ")"
 *                  | "true" | "false"
 *                  | "required" <package-name>
 *                  | "environment" <variable-name>
 *                  | "version" <version-condition>
 *                  | [<option-name>] ("true" | "false" | "enabled" | "disabled")
 * @endcode
 *
 * Uses recursive-descent parsing to validate the tokenized expression without
 * evaluating it. After the top-level parse completes, it verifies that every
 * token was consumed.
 *
 * @param expression The raw condition expression string to validate.
 * @param node       The YAML node from which the expression originated,
 *                   used for error reporting.
 * @param path       The YAML path to the node, used for error reporting.
 * @param context    The parser context providing error-reporting utilities.
 *
 * @note Terminates the program via `fail_config` on any of the following:
 *       - The expression is empty.
 *       - A parentheses mismatch is detected (missing opening or closing
 *         parenthesis).
 *       - An operand is missing after `required` or `environment`.
 *       - A version comparison is missing or syntactically invalid (delegated
 *         to `validate_version_condition`).
 *       - An option condition does not end with one of `true`, `false`,
 *         `enabled`, or `disabled`.
 *       - An unexpected or extra token remains after the parse completes.
 */
void validate_condition(const std::string& expression, const YAML::Node& node,
                        const std::string& path, const DatabaseParserContext& context) {
    const std::vector<std::string> tokens = tokenize_condition(expression, node, path, context);
    if (tokens.empty()) {
        fail_config(node, path, "must not be empty", context);
    }

    std::size_t position = 0;
    std::function<void()> parse_or;
    std::function<void()> parse_and;
    std::function<void()> parse_unary;
    std::function<void()> parse_primary;

    // Parses a primary expression: parenthesized sub-expression, a boolean
    // literal (true/false), a required-package condition, an environment-
    // variable condition, a version comparison, or an option condition
    // (optionally preceded by an option name).
    parse_primary = [&] {
        if (position >= tokens.size()) {
            fail_config(node, path, "ends before a condition was provided", context);
        }
        if (tokens[position] == "(") {
            ++position;
            parse_or();
            if (position >= tokens.size() || tokens[position] != ")") {
                fail_config(node, path, "contains unmatched parentheses", context);
            }
            ++position;
            return;
        }
        if (tokens[position] == "true" || tokens[position] == "false") {
            ++position;
            return;
        }
        if (tokens[position] == "required" || tokens[position] == "environment") {
            ++position;
            if (position >= tokens.size() || tokens[position] == "(" || tokens[position] == ")" ||
                tokens[position] == "&&" || tokens[position] == "||") {
                fail_config(node, path, "is missing an operand", context);
            }
            ++position;
            return;
        }
        if (tokens[position] == "version") {
            ++position;
            if (position >= tokens.size()) {
                fail_config(node, path, "is missing a version comparison", context);
            }
            validate_version_condition(tokens[position], node, path, context);
            ++position;
            return;
        }

        // Option condition: optionally preceded by an option name token.
        ++position;
        if (position >= tokens.size() ||
            (tokens[position] != "true" && tokens[position] != "false" &&
             tokens[position] != "enabled" && tokens[position] != "disabled")) {
            fail_config(node, path, "contains an invalid option condition", context);
        }
        ++position;
        if (position < tokens.size() && tokens[position] != "&&" && tokens[position] != "||" &&
            tokens[position] != ")") {
            ++position;
        }
    };

    // Parses a unary expression: optionally prefixed by one or more "not"
    // operators, followed by a primary expression.
    parse_unary = [&] {
        if (position < tokens.size() && tokens[position] == "not") {
            ++position;
            parse_unary();
        } else {
            parse_primary();
        }
    };

    // Parses a conjunction: a unary expression followed by zero or more
    // "&&" unary-expression pairs.
    parse_and = [&] {
        parse_unary();
        while (position < tokens.size() && tokens[position] == "&&") {
            ++position;
            parse_unary();
        }
    };

    // Parses a disjunction: an and-expression followed by zero or more
    // "||" and-expression pairs.
    parse_or = [&] {
        parse_and();
        while (position < tokens.size() && tokens[position] == "||") {
            ++position;
            parse_and();
        }
    };

    parse_or();
    if (position != tokens.size()) {
        fail_config(node, path, "contains unexpected token '" + tokens[position] + "'", context);
    }
}

/**
 * @brief Recursively validates that all template variables in a YAML subtree
 *        are syntactically well-formed.
 *
 * Walks every node (maps, sequences, and scalars) in the YAML subtree rooted
 * at `node`. For each scalar value, it scans for `${...}` template-variable
 * references and checks that:
 *   - Every opening `${` has a matching `}`.
 *   - No template variable is nested inside another (i.e. `${...${...}...}`).
 *   - No template variable is empty (`${}`).
 *
 * @param node    The YAML node whose subtree should be validated.
 * @param path    The YAML path to the current node, used for error reporting.
 * @param context The parser context providing error-reporting utilities.
 *
 * @note Terminates the program via `fail_config` if any malformed template
 *       variable is found.
 * @warning This function does **not** evaluate or resolve template variables;
 *          it only checks for syntactic correctness.
 */
void validate_templates(const YAML::Node& node, const std::string& path,
                        const DatabaseParserContext& context) {
    if (node.IsMap()) {
        for (const auto& entry : node) {
            validate_templates(entry.first, path + ".<key>", context);
            const std::string key = entry.first.IsScalar() ? entry.first.Scalar() : "<value>";
            validate_templates(entry.second, path + "." + key, context);
        }
        return;
    }
    if (node.IsSequence()) {
        for (std::size_t i = 0; i < node.size(); ++i) {
            validate_templates(node[i], path + "[" + std::to_string(i) + "]", context);
        }
        return;
    }
    if (!node.IsScalar()) {
        return;
    }

    const std::string value = node.Scalar();
    std::size_t position    = 0;
    while ((position = value.find("${", position)) != std::string::npos) {
        const std::size_t closing = value.find('}', position + 2);
        const std::size_t nested  = value.find('{', position + 2);
        if (closing == std::string::npos) {
            fail_config(node, path, "contains an unclosed template variable", context);
        }
        if (nested != std::string::npos && nested < closing) {
            fail_config(node, path, "contains a nested template variable", context);
        }
        if (closing == position + 2) {
            fail_config(node, path, "contains an empty template variable", context);
        }
        position = closing + 1;
    }
}
