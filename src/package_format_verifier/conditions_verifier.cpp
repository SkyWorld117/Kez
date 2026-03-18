#include <package_format_verifier/conditions_verifier.hpp>

// condition =
//     "required" <dependency> |
//     "environment" <variable> |
//     <option> <enabled:bool> [<value>] |
//     "version" <version_range> |
//     <condition> && <condition> |
//     <condition> || <condition> |
//     "not" <condition> |
//     "(" <condition> ")"

bool verify_condition_format(std::vector<std::string> tokens);
bool verify_condition_format(std::string condition) {
    if (condition.empty()) {
        ERROR("Condition cannot be empty.");
        return false;
    }

    // Split the condition into tokens
    std::vector<std::string> tokens;
    std::string token;
    char last_char = ' ';
    for (char c : condition) {
        if (isspace(c) || c == '(' || c == ')' || c == '&' || c == '|') {
            if ((c == '&' || c == '|') && last_char == c) {
                token += c;
            }
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
            if (c == '(' || c == ')') {
                tokens.push_back(std::string(1, c));
            }
            if ((c == '&' || c == '|') && last_char != c) {
                token += c;
            }
        } else {
            token += c;
        }
        last_char = c;
    }
    // Add the last token if it exists
    if (!token.empty()) {
        tokens.push_back(token);
    }

    return verify_condition_format(tokens);
}

bool verify_condition_format(std::vector<std::string> tokens) {
    if (tokens.empty()) {
        ERROR("Condition cannot be empty.");
        return false;
    }

    if (tokens.size() == 1) {
        return false;  // Single token conditions are not valid
    }

    // Handle parentheses - check if entire expression is wrapped in a single pair
    if (tokens[0] == "(") {
        int paren_count    = 0;
        size_t closing_pos = 0;
        for (size_t i = 0; i < tokens.size(); i++) {
            if (tokens[i] == "(") {
                paren_count++;
            } else if (tokens[i] == ")") {
                paren_count--;
                if (paren_count == 0) {
                    closing_pos = i;
                    break;
                }
            }
        }
        // If the entire expression is wrapped in parentheses
        if (closing_pos == tokens.size() - 1) {
            std::vector<std::string> inner_tokens(tokens.begin() + 1, tokens.end() - 1);
            return verify_condition_format(inner_tokens);
        }
    }

    // Handle logical operators (&&, ||) - lowest precedence
    // Only consider operators that are not inside parentheses
    for (size_t i = 1; i < tokens.size() - 1; i++) {
        if (tokens[i] == "&&" || tokens[i] == "||") {
            // Check if this operator is at the top level (not inside parentheses)
            int paren_count   = 0;
            bool is_top_level = true;
            for (size_t j = 0; j < i; j++) {
                if (tokens[j] == "(") {
                    paren_count++;
                } else if (tokens[j] == ")") {
                    paren_count--;
                }
            }
            if (paren_count == 0) {
                std::vector<std::string> left_tokens(tokens.begin(), tokens.begin() + i);
                std::vector<std::string> right_tokens(tokens.begin() + i + 1, tokens.end());
                return verify_condition_format(left_tokens) &&
                       verify_condition_format(right_tokens);
            }
        }
    }

    // Handle "not" operator
    if (tokens[0] == "not") {
        if (tokens.size() < 2) {
            ERROR("'not' operator requires a condition to negate.");
            return false;
        }
        std::vector<std::string> remaining_tokens(tokens.begin() + 1, tokens.end());
        return verify_condition_format(remaining_tokens);
    }

    // Handle "required" condition
    if (tokens[0] == "required") {
        if (tokens.size() != 2) {
            ERROR("'required' condition must have exactly one dependency.");
            return false;
        }
        // Check that dependency is a single string without whitespace
        for (char c : tokens[1]) {
            if (isspace(c)) {
                ERROR("Dependency name cannot contain whitespace.");
                return false;
            }
        }
        return true;
    }

    // Handle "environment" condition
    if (tokens[0] == "environment") {
        if (tokens.size() != 2) {
            ERROR("'environment' condition must have exactly one variable.");
            return false;
        }
        // Check that variable is a single string without whitespace
        for (char c : tokens[1]) {
            if (isspace(c)) {
                ERROR("Environment variable name cannot contain whitespace.");
                return false;
            }
        }
        return true;
    }

    // Handle "version" condition
    if (tokens[0] == "version") {
        if (tokens.size() != 2) {
            ERROR("'version' condition must have exactly one version range.");
            return false;
        }
        // Check that version_range is a single string without whitespace
        for (char c : tokens[1]) {
            if (isspace(c)) {
                ERROR("Version range cannot contain whitespace.");
                return false;
            }
        }
        return true;
    }

    // Handle option condition: <option> <enabled:bool> [<value>]
    if (tokens.size() >= 2) {
        // Check if second token is a boolean
        if (tokens[1] == "true" || tokens[1] == "false") {
            // Validate option name (first token)
            for (char c : tokens[0]) {
                if (isspace(c)) {
                    ERROR("Option name cannot contain whitespace.");
                    return false;
                }
            }

            // Option with boolean only
            if (tokens.size() == 2) {
                return true;
            }
            // Option with boolean and value
            else if (tokens.size() == 3) {
                // Check that value is a single string without whitespace
                for (char c : tokens[2]) {
                    if (isspace(c)) {
                        ERROR("Option value cannot contain whitespace.");
                        return false;
                    }
                }
                return true;
            } else {
                ERROR("Option condition can have at most 3 tokens: option, enabled, and optional "
                      "value.");
                return false;
            }
        }
    }

    ERROR("Invalid condition format.");
    return false;
}

bool verify_conditions(const YAML::Node& node) {
    if (!node.IsSequence()) {
        ERROR("Conditions must be a sequence.");
        return false;
    }

    for (const auto& condition : node) {
        if (!condition["condition"] || !condition["condition"].IsScalar()) {
            ERROR("Each condition must have a 'condition' key with a scalar value.");
            return false;
        }
        if (!condition["value"] || !condition["value"].IsScalar()) {
            ERROR("Each condition must have a 'value' key with a scalar value.");
            return false;
        }
        std::string condition_str = condition["condition"].as<std::string>();
        if (!verify_condition_format(condition_str)) {
            ERROR("Invalid condition format: " + condition_str);
            return false;
        }
    }

    return true;
}