/**
 * @file condition_evaluator.cpp
 * @brief Evaluation of parsed condition expressions for user-configuration parsing.
 */

#include <algorithm>
#include <cstdlib>
#include <database/config.hpp>
#include <dependency_resolver/requirements.hpp>
#include <parser/parser_internal.hpp>
#include <string>
#include <utils/condition_utils.hpp>
#include <utils/string_utils.hpp>

namespace {

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

    /**
     * @brief Evaluate a parsed condition AST against the current parser context.
     *
     * Walks the @ref ConditionExpr tree recursively.  Leaf nodes are resolved
     * against @p context (option states, environment variables, dependency set,
     * version comparisons).  Internal nodes (Or, And, Not) combine sub-results
     * with the usual boolean logic.
     */
    bool evaluate_condition_expr(const ConditionExpr& expr, UserConfigParserContext& context) {
        switch (expr.type) {
            case ConditionExpr::Or:
                for (const auto& child : expr.children) {
                    if (evaluate_condition_expr(child, context)) {
                        return true;
                    }
                }
                return false;

            case ConditionExpr::And:
                for (const auto& child : expr.children) {
                    if (!evaluate_condition_expr(child, context)) {
                        return false;
                    }
                }
                return true;

            case ConditionExpr::Not: return !evaluate_condition_expr(expr.children[0], context);

            case ConditionExpr::Literal: return expr.literal_value;

            case ConditionExpr::Required:
                return requirements_satisfied({expr.arg1}, context.dependencies,
                                              context.abstract_packages);

            case ConditionExpr::Environment: {
                const std::string name = strip_condition_template(expr.arg1);
                const auto parsed      = context.named_environment_values.find(name);
                if (parsed != context.named_environment_values.end()) {
                    return !parsed->second.empty();
                }
                const char* value = std::getenv(name.c_str());
                return value != nullptr && *value != '\0';
            }

            case ConditionExpr::Version: return compare_version_expression(expr.arg1, context);

            case ConditionExpr::Option: {
                const std::string option_name    = strip_condition_template(expr.arg1);
                const std::string expected_state = expr.arg2;
                const auto option                = context.named_option_values.find(option_name);
                if (option == context.named_option_values.end()) {
                    if (is_declared_abstract_selector(option_name, context)) {
                        return false;
                    }
                    user_config_error("condition references unresolved option '" + option_name +
                                      "'");
                }

                const bool expected_enabled =
                    expected_state == "true" || expected_state == "enabled";
                const bool expected_disabled =
                    expected_state == "false" || expected_state == "disabled";
                if (!expected_enabled && !expected_disabled) {
                    user_config_error("condition has invalid option state '" + expected_state +
                                      "'");
                }
                bool result = option->second.enabled == expected_enabled;
                if (!expr.arg3.empty()) {
                    result = result && get_selected_option_value(option->second) == expr.arg3;
                }
                return result;
            }
        }
        return false;  // unreachable
    }

}  // namespace

bool evaluate_parser_condition(const std::string& expression, UserConfigParserContext& context) {
    // Cache the parsed AST so that the fixed-point convergence loop does not
    // re-tokenise and re-parse the same expression on every pass.
    auto it = context.condition_parse_cache.find(expression);
    if (it == context.condition_parse_cache.end()) {
        auto on_error = [](const std::string& msg) { user_config_error(msg); };
        it =
            context.condition_parse_cache.emplace(expression, parse_condition(expression, on_error))
                .first;
    }
    return evaluate_condition_expr(it->second, context);
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
