#pragma once

#include <cctype>
#include <string>
#include <vector>

/**
 * @brief Parsed representation of a condition expression.
 *
 * Produced by parse_condition_tokens() / parse_condition().  An instance
 * can represent an entire expression tree (Or / And / Not) or a single
 * leaf atom (Literal / Required / Environment / Version / Option).
 *
 * Leaf nodes store their operands as raw token strings, preserving any
 * template markers (e.g. @c ${prefix}).  Callers that need to resolve
 * templates (user-config parser) do so during tree evaluation; callers
 * that only validate syntax (database parser) inspect the raw strings.
 */
struct ConditionExpr {
    /** @brief Node kind in the condition AST. */
    enum Type {
        Or,           ///< Logical OR of @ref children.
        And,          ///< Logical AND of @ref children.
        Not,          ///< Logical NOT of @ref children[0].
        Literal,      ///< Boolean constant (@ref literal_value).
        Required,     ///< @c required @p arg1  (package-name dependency check).
        Environment,  ///< @c environment @p arg1  (environment-variable check).
        Version,      ///< @c version @p arg1  (version-range comparison).
        Option        ///< [@p arg1] @p arg2 [@p arg3]  (option-state check).
    };

    Type type = Literal;

    // ── Leaf data (meaning varies by Type) ──────────────────────────

    /** For @c Literal: the boolean constant. */
    bool literal_value = false;

    /**
     * For @c Required:   package name.
     * For @c Environment: environment variable name (possibly a ${…} template).
     * For @c Version:     version comparison expression (e.g. @c ${pkg.version}>=1.0).
     * For @c Option:      option name (possibly a ${…} template, or empty).
     */
    std::string arg1;

    /** For @c Option: the state token (true/false/enabled/disabled). */
    std::string arg2;

    /** For @c Option: optional value token after the state. */
    std::string arg3;

    // ── Compound-node children ──────────────────────────────────────

    /** For @c Or/@c And: the disjuncts/conjuncts (n-ary).
     *  For @c Not: exactly one child.
     *  For leaf types: empty. */
    std::vector<ConditionExpr> children;
};

// -----------------------------------------------------------------------
// Tokenizer
// -----------------------------------------------------------------------

/**
 * @brief Splits a condition expression into a token vector.
 *
 * Whitespace separates tokens.  Parentheses become single-character tokens;
 * @c && and @c || become two-character tokens; everything else is grouped
 * into an operand token.
 *
 * @tparam ErrorFunc  Callable<void(const std::string&)> that terminates
 *                    (e.g. by calling @c ERROR or @c fail_config).  Called
 *                    when a lone @c & or @c | is found (incomplete operator).
 * @param expression  Raw condition string.
 * @param on_error    Invoked with a descriptive message on fatal syntax errors.
 * @return Tokens in left-to-right order.
 */
template <typename ErrorFunc>
std::vector<std::string> tokenize_condition(const std::string& expression, ErrorFunc on_error) {
    std::vector<std::string> tokens;
    std::string current;

    auto flush = [&] {
        if (!current.empty()) {
            tokens.push_back(current);
            current.clear();
        }
    };

    for (std::size_t i = 0; i < expression.size(); ++i) {
        const char c = expression[i];
        if (std::isspace(static_cast<unsigned char>(c))) {
            flush();
        } else if (c == '(' || c == ')') {
            flush();
            tokens.emplace_back(1, c);
        } else if (c == '&' || c == '|') {
            flush();
            if (i + 1 >= expression.size() || expression[i + 1] != c) {
                on_error("condition contains an incomplete logical operator");
            }
            tokens.emplace_back(2, c);
            ++i;
        } else {
            current += c;
        }
    }
    flush();
    return tokens;
}

// -----------------------------------------------------------------------
// Recursive-descent parser (tokens → ConditionExpr AST)
// -----------------------------------------------------------------------

namespace detail {

    template <typename ErrorFunc> struct ConditionParserImpl {
        const std::vector<std::string>& tokens;
        std::size_t pos = 0;
        ErrorFunc on_error;

        // or → and ("||" and)*
        ConditionExpr parse_or() {
            ConditionExpr expr;
            expr.type = ConditionExpr::Or;
            expr.children.push_back(parse_and());

            while (pos < tokens.size() && tokens[pos] == "||") {
                ++pos;
                expr.children.push_back(parse_and());
            }

            if (expr.children.size() == 1) {
                return std::move(expr.children[0]);
            }
            return expr;
        }

        // and → unary ("&&" unary)*
        ConditionExpr parse_and() {
            ConditionExpr expr;
            expr.type = ConditionExpr::And;
            expr.children.push_back(parse_unary());

            while (pos < tokens.size() && tokens[pos] == "&&") {
                ++pos;
                expr.children.push_back(parse_unary());
            }

            if (expr.children.size() == 1) {
                return std::move(expr.children[0]);
            }
            return expr;
        }

        // unary → "not" unary | primary
        ConditionExpr parse_unary() {
            if (pos < tokens.size() && tokens[pos] == "not") {
                ++pos;
                ConditionExpr expr;
                expr.type = ConditionExpr::Not;
                expr.children.push_back(parse_unary());
                return expr;
            }
            return parse_primary();
        }

        // primary → "(" or ")"
        //         | "true" | "false"
        //         | "required" name
        //         | "environment" name
        //         | "version" comparison
        //         | [name] ("true" | "false" | "enabled" | "disabled") [name]
        ConditionExpr parse_primary() {
            if (pos >= tokens.size()) {
                on_error("condition ends before an operand was provided");
            }

            const std::string& tok = tokens[pos];

            // Parenthesised sub-expression
            if (tok == "(") {
                ++pos;
                ConditionExpr expr = parse_or();
                if (pos >= tokens.size() || tokens[pos] != ")") {
                    on_error("condition contains unmatched parentheses");
                }
                ++pos;
                return expr;
            }

            // Boolean literal
            if (tok == "true" || tok == "false") {
                ++pos;
                ConditionExpr expr;
                expr.type          = ConditionExpr::Literal;
                expr.literal_value = (tok == "true");
                return expr;
            }

            // "required" name
            if (tok == "required") {
                ++pos;
                if (pos >= tokens.size() || tokens[pos] == "(" || tokens[pos] == ")" ||
                    tokens[pos] == "&&" || tokens[pos] == "||") {
                    on_error("required condition is missing a package name");
                }
                ConditionExpr expr;
                expr.type = ConditionExpr::Required;
                expr.arg1 = tokens[pos];
                ++pos;
                return expr;
            }

            // "environment" name
            if (tok == "environment") {
                ++pos;
                if (pos >= tokens.size() || tokens[pos] == "(" || tokens[pos] == ")" ||
                    tokens[pos] == "&&" || tokens[pos] == "||") {
                    on_error("environment condition is missing a variable name");
                }
                ConditionExpr expr;
                expr.type = ConditionExpr::Environment;
                expr.arg1 = tokens[pos];
                ++pos;
                return expr;
            }

            // "version" comparison
            if (tok == "version") {
                ++pos;
                if (pos >= tokens.size()) {
                    on_error("version condition is missing a comparison");
                }
                ConditionExpr expr;
                expr.type = ConditionExpr::Version;
                expr.arg1 = tokens[pos];
                ++pos;
                return expr;
            }

            // Option condition: [name] state [value]
            {
                ConditionExpr expr;
                expr.type = ConditionExpr::Option;
                expr.arg1 = tokens[pos];
                ++pos;

                if (pos >= tokens.size() ||
                    (tokens[pos] != "true" && tokens[pos] != "false" && tokens[pos] != "enabled" &&
                     tokens[pos] != "disabled")) {
                    on_error("option condition is missing an enabled/disabled state");
                }
                expr.arg2 = tokens[pos];
                ++pos;

                // Optional value token (anything that isn't an operator or paren)
                if (pos < tokens.size() && tokens[pos] != "&&" && tokens[pos] != "||" &&
                    tokens[pos] != ")") {
                    expr.arg3 = tokens[pos];
                    ++pos;
                }
                return expr;
            }
        }
    };

}  // namespace detail

/**
 * @brief Parse a pre-tokenized condition expression into an AST.
 *
 * Validates the full grammar; on any structural error @p on_error is called
 * with a descriptive message and must not return (the function uses it to
 * terminate).
 *
 * @tparam ErrorFunc  Callable<void(const std::string&)> that terminates.
 * @param tokens      Token vector from tokenize_condition().
 * @param on_error    Fatal error handler.
 * @return The root of the parsed condition AST.
 */
template <typename ErrorFunc>
ConditionExpr parse_condition_tokens(const std::vector<std::string>& tokens, ErrorFunc on_error) {
    detail::ConditionParserImpl<ErrorFunc> parser {tokens, 0, on_error};
    ConditionExpr result = parser.parse_or();
    if (parser.pos != tokens.size()) {
        on_error("condition contains unexpected token '" + tokens[parser.pos] + "'");
    }
    return result;
}

/**
 * @brief Tokenize and parse a condition expression in one call.
 *
 * Equivalent to calling tokenize_condition() followed by
 * parse_condition_tokens().
 *
 * @tparam ErrorFunc  Callable<void(const std::string&)> that terminates.
 * @param expression  Raw condition string.
 * @param on_error    Fatal error handler.
 * @return The root of the parsed condition AST.
 */
template <typename ErrorFunc>
ConditionExpr parse_condition(const std::string& expression, ErrorFunc on_error) {
    std::vector<std::string> tokens = tokenize_condition(expression, on_error);
    if (tokens.empty()) {
        on_error("condition must not be empty");
    }
    return parse_condition_tokens(tokens, on_error);
}
