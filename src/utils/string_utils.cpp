#include <algorithm>
#include <cctype>
#include <string>
#include <utils/colored_io.hpp>
#include <utils/string_utils.hpp>

/** @brief Splits a string on a single character delimiter, discarding empty tokens. */
std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    size_t pos = 0;
    size_t del_pos;
    while ((del_pos = str.find(delimiter, pos)) != std::string::npos) {
        std::string token = str.substr(pos, del_pos - pos);
        if (!token.empty()) {
            tokens.push_back(token);
        }
        pos = del_pos + 1;  // Move past the delimiter
    }
    // Add the remainder after the last delimiter (if non-empty)
    if (!str.empty()) {
        tokens.push_back(str.substr(pos));
    }
    return tokens;
}

/** @brief Splits a string on any of the given delimiter characters, discarding empty tokens. */
std::vector<std::string> split(const std::string& str, const std::vector<char>& delimiters) {
    std::vector<std::string> tokens;
    size_t pos = 0;
    size_t del_pos;
    while (pos < str.size()) {
        // Find the earliest occurrence of any delimiter from the current
        // position.
        del_pos = std::string::npos;
        for (char delim : delimiters) {
            size_t temp_pos = str.find(delim, pos);
            if (temp_pos != std::string::npos &&
                (del_pos == std::string::npos || temp_pos < del_pos)) {
                del_pos = temp_pos;
            }
        }
        if (del_pos != std::string::npos) {
            std::string token = str.substr(pos, del_pos - pos);
            if (!token.empty()) {
                tokens.push_back(token);
            }
            pos = del_pos + 1;  // Move past the delimiter
        } else {
            break;  // No more delimiters found
        }
    }
    // Add the remainder after the last delimiter (if non-empty)
    if (pos < str.size()) {
        tokens.push_back(str.substr(pos));
    }
    return tokens;
}

/** @brief Splits a string on the earliest of the given delimiter characters, keeping each delimiter as its own token. */
std::vector<std::string> split_keep_delimiters(const std::string& str,
                                               const std::vector<char>& delimiters) {
    std::vector<std::string> tokens;
    size_t pos = 0;
    size_t del_pos;
    while (pos < str.size()) {
        // Find the earliest occurrence of any delimiter.
        del_pos          = std::string::npos;
        char found_delim = '\0';
        for (char delim : delimiters) {
            size_t temp_pos = str.find(delim, pos);
            if (temp_pos != std::string::npos &&
                (del_pos == std::string::npos || temp_pos < del_pos)) {
                del_pos     = temp_pos;
                found_delim = delim;
            }
        }
        if (del_pos != std::string::npos) {
            std::string token = str.substr(pos, del_pos - pos);
            if (!token.empty()) {
                tokens.push_back(token);
            }
            // Insert the delimiter itself as a separate token.
            tokens.push_back(std::string(1, found_delim));
            pos = del_pos + 1;  // Move past the delimiter
        } else {
            break;  // No more delimiters found
        }
    }
    // Add the remainder after the last delimiter (if non-empty)
    if (pos < str.size()) {
        tokens.push_back(str.substr(pos));
    }
    return tokens;
}

/** @brief Checks whether every character in the string is alphabetic. */
bool is_alphabetic(const std::string& str) {
    for (char c : str) {
        if (!is_alphabetic(c)) {
            return false;
        }
    }
    return true;
}

/** @brief Checks whether a single character is alphabetic using the C locale. */
bool is_alphabetic(char c) { return std::isalpha(static_cast<unsigned char>(c)); }

/** @brief Checks whether every character in the string is a digit. */
bool is_numeric(const std::string& str) {
    for (char c : str) {
        if (!is_numeric(c)) {
            return false;
        }
    }
    return true;
}

/** @brief Checks whether a single character is a digit using the C locale. */
bool is_numeric(char c) { return std::isdigit(static_cast<unsigned char>(c)); }

/** @brief Returns the length of the string with ANSI escape sequences stripped out. */
int get_length_without_color(const std::string& str) {
    char delim = '\033';

    size_t size = 0;
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == delim) {
            // Skip all characters until the terminating 'm' of the escape
            // sequence.
            for (; i < str.size(); ++i) {
                if (str[i] == 'm') {
                    break;
                }
            }
        } else {
            size += 1;
        }
    }

    return size;
}

/** @brief Compares two version strings segment-by-segment, returning -1, 0, or 1. */
int compare_versions(const std::string& left, const std::string& right) {
    std::vector<std::string> left_parts  = split(left, '.');
    std::vector<std::string> right_parts = split(right, '.');

    // Delimiters for the second split: every lowercase letter acts as a split
    // point, so that e.g. "10a" becomes ["10", "a"].
    static const std::vector<char> secondary_delimiters = {
        'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
        'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'};

    for (size_t i = 0; i < std::min(left_parts.size(), right_parts.size()); i++) {
        if (left_parts[i] != right_parts[i]) {
            // Split the differing segment further on alphabetic boundaries.
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
                        // At least one subpart is non-numeric; compare
                        // lexicographically.
                        return left_subparts[j] < right_subparts[j] ? -1 : 1;
                    }
                }
            }
            // All compared subparts are equal; the shorter subpart sequence
            // is considered smaller.
            return left_subparts.size() < right_subparts.size() ? -1 : 1;
        }
    }

    // All common top-level segments are equal; the version with fewer
    // segments is smaller.
    if (left_parts.size() != right_parts.size()) {
        return left_parts.size() < right_parts.size() ? -1 : 1;
    }

    return 0;
}

/** @brief Strips leading and trailing whitespace (space, newline, carriage-return, tab) from the input string. */
std::string trim(const std::string& input) {
    const std::size_t first = input.find_first_not_of(" \n\r\t");
    if (first == std::string::npos) {
        return {};
    }
    const std::size_t last = input.find_last_not_of(" \n\r\t");
    return input.substr(first, last - first + 1);
}

/** @brief Appends @p value to @p values if it is non-empty and not already present. */
void append_unique(std::vector<std::string>& values, const std::string& value) {
    if (!value.empty() && std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

/** @brief Concatenates all strings in @p values separated by @p separator. */
std::string join(const std::vector<std::string>& values, const std::string& separator) {
    std::string result;
    for (const std::string& value : values) {
        result += (result.empty() ? "" : separator) + value;
    }
    return result;
}

/** @brief Checks whether @p name is a valid uppercase-underscore shell assignment variable name. */
bool is_shell_assignment(const std::string& name) {
    if (name.empty() || (name[0] != '_' && !std::isupper(static_cast<unsigned char>(name[0])))) {
        return false;
    }
    return std::all_of(name.begin() + 1, name.end(), [](const char character) {
        return character == '_' || std::isupper(static_cast<unsigned char>(character)) ||
               std::isdigit(static_cast<unsigned char>(character));
    });
}
