#include <database/condition_parser.hpp>
#include <database/parser_utils.hpp>
#include <string>
#include <utils/condition_utils.hpp>

namespace {

    /**
     * @brief Validates a version-comparison expression from a package recipe.
     *
     * Checks that the token begins with a template variable (${…}), that the
     * template is well-formed, and that the comma-separated comparison
     * clauses use valid operators (>=, <=, ==, !=, >, <).
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
        const std::string comparisons = token.substr(closing + 1);
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

    /**
     * @brief Recursively walks a ConditionExpr AST and validates every
     *        Version node's format.
     */
    void validate_version_nodes(const ConditionExpr& expr, const YAML::Node& node,
                                const std::string& path, const DatabaseParserContext& context) {
        if (expr.type == ConditionExpr::Version) {
            validate_version_condition(expr.arg1, node, path, context);
        }
        for (const auto& child : expr.children) {
            validate_version_nodes(child, node, path, context);
        }
    }

}  // namespace

void validate_condition(const std::string& expression, const YAML::Node& node,
                        const std::string& path, const DatabaseParserContext& context) {
    auto on_error = [&](const std::string& msg) { fail_config(node, path, msg, context); };

    const ConditionExpr expr = parse_condition(expression, on_error);
    // Additional validation specific to package-recipe conditions:
    // check that version comparisons are well-formed.
    validate_version_nodes(expr, node, path, context);
}

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
