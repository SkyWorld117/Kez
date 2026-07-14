/**
 * @file condition_evaluator.cpp
 * @brief Tokenization and evaluation of user-configuration conditions.
 */

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <database/config.hpp>
#include <dependency_resolver/requirements.hpp>
#include <parser/parser_internal.hpp>
#include <string>
#include <utils/string_utils.hpp>
#include <vector>

namespace {

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

    std::string strip_condition_template(const std::string& value) {
        if (value.size() >= 3 && value.rfind("${", 0) == 0 && value.back() == '}') {
            return value.substr(2, value.size() - 3);
        }
        return value;
    }

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
        const std::vector<std::string>& tokens;
        UserConfigParserContext& context;
        std::size_t position = 0;
    };

    bool parse_condition_or(ConditionCursor& cursor);

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
            const std::string name = strip_condition_template(cursor.tokens[cursor.position++]);
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
        const std::string option_name    = strip_condition_template(token);
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

    bool parse_condition_unary(ConditionCursor& cursor) {
        if (cursor.position < cursor.tokens.size() && cursor.tokens[cursor.position] == "not") {
            ++cursor.position;
            return !parse_condition_unary(cursor);
        }
        return parse_condition_primary(cursor);
    }

    bool parse_condition_and(ConditionCursor& cursor) {
        bool result = parse_condition_unary(cursor);
        while (cursor.position < cursor.tokens.size() && cursor.tokens[cursor.position] == "&&") {
            ++cursor.position;
            const bool right = parse_condition_unary(cursor);
            result           = result && right;
        }
        return result;
    }

    bool parse_condition_or(ConditionCursor& cursor) {
        bool result = parse_condition_and(cursor);
        while (cursor.position < cursor.tokens.size() && cursor.tokens[cursor.position] == "||") {
            ++cursor.position;
            const bool right = parse_condition_and(cursor);
            result           = result || right;
        }
        return result;
    }

}  // namespace

bool evaluate_parser_condition(const std::string& expression, UserConfigParserContext& context) {
    const std::vector<std::string> tokens = tokenize_condition(expression);
    if (tokens.empty()) {
        user_config_error("condition must not be empty");
    }
    ConditionCursor cursor {tokens, context};
    const bool result = parse_condition_or(cursor);
    if (cursor.position != tokens.size()) {
        user_config_error("condition contains unexpected token '" + tokens[cursor.position] + "'");
    }
    return result;
}

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
