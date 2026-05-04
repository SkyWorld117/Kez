#include <parser/conditions_parser.hpp>
#include <string>

std::vector<std::string> tokenizer(const std::string& str) {
    std::vector<std::string> tokens;

    char last_char = ' ';
    std::string token;
    for (char c : str) {
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
    if (!token.empty()) {
        tokens.push_back(token);
    }

    return tokens;
}

bool evaluate_condition(const std::vector<std::string>& tokens, const ParserContext& context) {
    // Unpack context
    const std::unordered_map<std::string, std::string>& template_map = context.template_map;
    const YAML::Node user_config                                    = context.user_config;

    if (tokens.empty()) {
        ERROR("Condition cannot be empty.");
        exit(EXIT_FAILURE);
    }

    if (tokens.size() == 1) {
        // Single token conditions are not valid
        ERROR("Single token conditions are not valid.");
        exit(EXIT_FAILURE);
    }

    // Handle parentheses
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
        if (closing_pos == tokens.size() - 1) {
            std::vector<std::string> inner_tokens(tokens.begin() + 1, tokens.end() - 1);
            return evaluate_condition(inner_tokens, context);
        } else {
            ERROR("Mismatched parentheses in condition.");
            exit(EXIT_FAILURE);
        }
    }

    // Handle logical operators
    for (size_t i = 1; i < tokens.size() - 1; i++) {
        if (tokens[i] == "&&" || tokens[i] == "||") {
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
                bool left_result  = evaluate_condition(left_tokens, context);
                bool right_result = evaluate_condition(right_tokens, context);
                if (tokens[i] == "&&") {
                    return left_result && right_result;
                } else if (tokens[i] == "||") {
                    return left_result || right_result;
                }
            }
        }
    }

    // Handle "not" operator
    if (tokens[0] == "not") {
        if (tokens.size() < 2) {
            ERROR("'not' operator requires a condition to negate.");
            exit(EXIT_FAILURE);
        }
        std::vector<std::string> remaining_tokens(tokens.begin() + 1, tokens.end());
        return !evaluate_condition(remaining_tokens, context);
    }

    // Handle "environment" condition
    if (tokens[0] == "environment") {
        if (tokens.size() != 2) {
            ERROR("'environment' condition must have exactly one variable.");
            exit(EXIT_FAILURE);
        }
        if (template_map.find(tokens[1]) != template_map.end()) {
            std::string env_var = template_map.at(tokens[1]).substr(
                2, template_map.at(tokens[1]).size() - 3);  // Remove ${ and }
            return !template_map.at(env_var).empty();
        } else {
            return false;
        }
    }

    // Handle "version" condition
    if (tokens[0] == "version") {
        if (tokens.size() != 2) {
            ERROR("'version' condition must have exactly one version range.");
            exit(EXIT_FAILURE);
        }
        // TODO: Implement version condition evaluation
        ERROR("Not implemented: version condition evaluation.");
        exit(EXIT_FAILURE);
    }

    // Handle option condition
    if (tokens[0].find("${") == 0) {
        if (tokens.size() < 2) {
            ERROR("Option condition requires at least the option name and enabled value.");
            exit(EXIT_FAILURE);
        }
        std::string option_name = tokens[0].substr(2, tokens[0].size() - 3);
        // If option not found, check if it starts with `{abstract_package}.use-{any_package_name}` and the abstract package is not recipe
        // If so, ignore the condition (return false), else throw error
        if (template_map.find(option_name) == template_map.end()) {
            if (option_name.find(".use-") != std::string::npos) {
                std::string abstract_pkg = option_name.substr(0, option_name.find(".use-"));
                // Verify if abstract_pkg exists in the database as an abstract package
                YAML::Node abstract_pkg_node = get_db_config(abstract_pkg);
                if (abstract_pkg_node.IsDefined() &&
                    abstract_pkg_node["cheese"]["type"].as<std::string>() == "abstract" &&
                    user_config["recipe"]["abstract_packages"]) {
                    std::unordered_map<std::string, std::string> recipe_abstracts =
                        user_config["recipe"]["abstract_packages"]
                            .as<std::unordered_map<std::string, std::string>>();
                    if (recipe_abstracts.find(abstract_pkg) == recipe_abstracts.end()) {
                        return false;  // Ignore the condition
                    }
                }
            }
            ERROR("Option '" + option_name + "' not found in template map.");
            exit(EXIT_FAILURE);
        }
        std::string option_value = template_map.at(option_name);
        // Check if enabled status matches the second token
        int dot_index              = option_value.find('.');
        std::string enabled_status = option_value.substr(0, dot_index);
        if (enabled_status != tokens[1]) {
            return false;  // Enabled status does not match
        }
        // If a value is provided, check if it matches
        if (tokens.size() == 3) {
            std::string expected_value = tokens[2];
            std::string actual_value   = option_value.substr(dot_index + 1);
            return (actual_value == expected_value);
        }
        return true;  // Only enabled status was checked
    }

    // If we reach here, the condition is not recognized
    std::string error_tokens;
    for (const auto& token : tokens) {
        error_tokens += token + " ";
    }
    ERROR("Unrecognized condition: " + error_tokens);
    exit(EXIT_FAILURE);
}

std::string parse_conditions(const std::string& base_value, const YAML::Node& conditions_node,
                             const ParserContext& context) {
    std::string result = base_value;

    for (const auto& condition : conditions_node) {
        std::string condition_str       = condition["condition"].as<std::string>();
        std::vector<std::string> tokens = tokenizer(condition_str);
        bool condition_result           = evaluate_condition(tokens, context);
        if (condition_result) {
            if (condition["action"]) {
                std::string action = condition["action"].as<std::string>();
                if (action == "set") {
                    result = condition["value"].as<std::string>();
                    break;  // No need to check further conditions
                } else if (action == "append") {
                    result += " " + condition["value"].as<std::string>();
                } else if (action == "prepend") {
                    result = condition["value"].as<std::string>() + " " + result;
                } else {
                    ERROR("Unknown action in condition: " + action);
                    exit(EXIT_FAILURE);
                }
            } else {
                // Default action is to set the value
                result = condition["value"].as<std::string>();
                break;  // No need to check further conditions
            }
        }
    }

    return result;
}