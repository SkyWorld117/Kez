#include "templates_verifier.h"

// Go through all the scalar entries to check if there exists a template entry
// and whether it is a valid template
bool verify_templates(const YAML::Node& node) {
    if (node.IsNull()) {
        return true;
    }

    if (node.IsMap()) {
        for (const auto& item : node) {
            // For maps, check both key and value
            if (!verify_templates(item.first) || !verify_templates(item.second)) {
                return false;
            }
        }
        return true;
    } else if (node.IsSequence()) {
        for (const auto& item : node) {
            if (!verify_templates(item)) {
                return false;
            }
        }
        return true;
    } else if (node.IsScalar()) {
        std::string str = node.as<std::string>();
        size_t pos      = 0;
        while ((pos = str.find("${", pos)) != std::string::npos) {
            // size_t end_pos = str.find('}', pos + 2);
            size_t end_pos;
            for (end_pos = pos + 2; end_pos != std::string::npos; end_pos++) {
                if (str[end_pos] == '{') {
                    ERROR("Error at line " + std::to_string(node.Mark().line) + ": " + str +
                          "\n"
                          "An unclosed variable template starting at position " +
                          std::to_string(pos) + " or a nested template at position " +
                          std::to_string(end_pos) + " was found.");
                    return false;
                }
                if (str[end_pos] == '}') {
                    break;
                }
            }
            if (end_pos == std::string::npos) {
                ERROR("Unclosed variable template in string at line " +
                      std::to_string(node.Mark().line) + ": " + str);
                return false;
            }

            // Extract the variable name between ${...}
            std::string var_name = str.substr(pos + 2, end_pos - pos - 2);

            // Check if variable name is empty
            if (var_name.empty()) {
                ERROR("Empty variable template ${} in string at line " +
                      std::to_string(node.Mark().line) + ": " + str);
                return false;
            }

            pos = end_pos + 1;  // Move past the closing brace
        }
        return true;
    }

    return false;
}