#include <algorithm>
#include <cctype>
#include <parser/conditions_parser.hpp>
#include <parser/template_parser.hpp>
#include <string>
#include <utils/string_utils.hpp>
#include <vector>

// struct VersionSegment {
//     long long number       = 0;
//     std::string alpha;
//     long long alpha_number = 0;
// };

// VersionSegment parse_version_segment(const std::string& segment) {
//     VersionSegment result;
//     size_t i = 0;
//     while (i < segment.size() && std::isdigit(static_cast<unsigned char>(segment[i]))) {
//         i++;
//     }
//     if (i > 0) {
//         result.number = std::stoll(segment.substr(0, i));
//     }

//     size_t alpha_start = i;
//     while (i < segment.size() && std::isalpha(static_cast<unsigned char>(segment[i]))) {
//         i++;
//     }
//     if (i > alpha_start) {
//         result.alpha = segment.substr(alpha_start, i - alpha_start);
//     }

//     size_t alpha_num_start = i;
//     while (i < segment.size() && std::isdigit(static_cast<unsigned char>(segment[i]))) {
//         i++;
//     }
//     if (i > alpha_num_start) {
//         result.alpha_number = std::stoll(segment.substr(alpha_num_start, i - alpha_num_start));
//     }

//     if (i != segment.size()) {
//         ERROR("Invalid version segment: " + segment);
//         exit(EXIT_FAILURE);
//     }
//     return result;
// }

// int compare_alpha_strings(const std::string& left, const std::string& right) {
//     size_t min_len = std::min(left.size(), right.size());
//     for (size_t i = 0; i < min_len; i++) {
//         char left_char  = static_cast<char>(std::tolower(static_cast<unsigned char>(left[i])));
//         char right_char = static_cast<char>(std::tolower(static_cast<unsigned char>(right[i])));
//         if (left_char != right_char) {
//             return left_char < right_char ? -1 : 1;
//         }
//     }
//     if (left.size() == right.size()) {
//         return 0;
//     }
//     return left.size() < right.size() ? -1 : 1;
// }

// int compare_version_segments(const VersionSegment& left, const VersionSegment& right) {
//     if (left.number != right.number) {
//         return left.number < right.number ? -1 : 1;
//     }

//     bool left_has_alpha  = !left.alpha.empty();
//     bool right_has_alpha = !right.alpha.empty();
//     if (left_has_alpha || right_has_alpha) {
//         if (!left_has_alpha) {
//             return 1;
//         }
//         if (!right_has_alpha) {
//             return -1;
//         }
//         int alpha_cmp = compare_alpha_strings(left.alpha, right.alpha);
//         if (alpha_cmp != 0) {
//             return alpha_cmp;
//         }
//         if (left.alpha_number != right.alpha_number) {
//             return left.alpha_number < right.alpha_number ? -1 : 1;
//         }
//     }
//     return 0;
// }

// int compare_versions(const std::string& left, const std::string& right) {
//     if (left.empty() || right.empty()) {
//         ERROR("Version strings cannot be empty.");
//         exit(EXIT_FAILURE);
//     }

//     std::vector<std::string> left_parts  = split(left, '.');
//     std::vector<std::string> right_parts = split(right, '.');
//     if (left_parts.empty() || right_parts.empty()) {
//         ERROR("Version strings must have at least one segment.");
//         exit(EXIT_FAILURE);
//     }

//     std::vector<VersionSegment> left_segments;
//     std::vector<VersionSegment> right_segments;

//     for (const auto& part : left_parts) {
//         VersionSegment segment = parse_version_segment(part);
//         left_segments.push_back(segment);
//     }

//     for (const auto& part : right_parts) {
//         VersionSegment segment = parse_version_segment(part);
//         right_segments.push_back(segment);
//     }

//     size_t max_size = left_segments.size() > right_segments.size() ? left_segments.size()
//                                                                   : right_segments.size();
//     for (size_t i = 0; i < max_size; i++) {
//         VersionSegment left_seg;
//         VersionSegment right_seg;
//         if (i < left_segments.size()) {
//             left_seg = left_segments[i];
//         }
//         if (i < right_segments.size()) {
//             right_seg = right_segments[i];
//         }
//         int seg_cmp = compare_version_segments(left_seg, right_seg);
//         if (seg_cmp != 0) {
//             return seg_cmp;
//         }
//     }
//     return 0;
// }

int compare_versions(const std::string& left, const std::string& right) {
    std::vector<std::string> left_parts  = split(left, '.');
    std::vector<std::string> right_parts = split(right, '.');

    static const std::vector<char> secondary_delimiters = {
        'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
        'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'};

    for (size_t i = 0; i < std::min(left_parts.size(), right_parts.size()); i++) {
        if (left_parts[i] != right_parts[i]) {
            // Split by alphabetic characters
            std::vector<std::string> left_subparts =
                split_keep_delimiters(left_parts[i], secondary_delimiters);
            std::vector<std::string> right_subparts =
                split_keep_delimiters(right_parts[i], secondary_delimiters);

            for (size_t j = 0; j < std::min(left_subparts.size(), right_subparts.size()); j++) {
                if (left_subparts[j] != right_subparts[j]) {
                    if (is_numeric(left_subparts[j]) && is_numeric(right_subparts[j])) {
                        long long left_num  = std::stoll(left_subparts[j]);
                        long long right_num = std::stoll(right_subparts[j]);
                        if (left_num != right_num) {
                            return left_num < right_num ? -1 : 1;
                        }
                    } else {
                        return left_subparts[j] < right_subparts[j] ? -1 : 1;
                    }
                }
            }
            return left_subparts.size() < right_subparts.size() ? -1 : 1;
        }
    }

    if (left_parts.size() != right_parts.size()) {
        return left_parts.size() < right_parts.size() ? -1 : 1;
    }

    return 0;
}

std::string get_version_operator(const std::string& condition) {
    static const std::vector<std::string> operators = {">=", "<=", "==", "!=", ">", "<"};
    for (const auto& candidate : operators) {
        if (condition.rfind(candidate) != std::string::npos) {
            return candidate;
        }
    }
    ERROR("No valid version operator found in condition: " + condition);
    exit(EXIT_FAILURE);
}

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

bool evaluate_condition(const std::vector<std::string>& tokens, ParserContext& context) {
    // Unpack context
    const std::unordered_map<std::string, std::string>& template_map = context.template_map;
    const YAML::Node user_config                                     = context.user_config;

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
        // Acceptable version range formats:
        // version ${pkg.version}<op><version>[,<op><version>]...]
        std::string version_condition = tokens[1];

        std::string template_name   = version_condition.substr(2, version_condition.find('}') - 2);
        std::string current_version = parse_template(template_name, context);

        version_condition = version_condition.substr(version_condition.find('}') + 1);

        std::vector<std::string> range_parts = split(version_condition, ',');
        if (range_parts.empty()) {
            ERROR("Version range cannot be empty.");
            exit(EXIT_FAILURE);
        }

        for (const std::string& part : range_parts) {
            std::string op = get_version_operator(part);

            std::string right = part.substr(op.size());
            if (right.empty()) {
                ERROR("Invalid version condition (missing comparison value): " + part);
                exit(EXIT_FAILURE);
            }

            int cmp = compare_versions(current_version, right);

            if ((op == ">" && !(cmp > 0)) || (op == ">=" && !(cmp >= 0)) ||
                (op == "<" && !(cmp < 0)) || (op == "<=" && !(cmp <= 0)) ||
                (op == "==" && !(cmp == 0)) || (op == "!=" && !(cmp != 0))) {
                return false;
            }
        }

        return true;
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
                             ParserContext& context) {
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