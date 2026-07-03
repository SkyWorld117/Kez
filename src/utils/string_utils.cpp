#include <string>
#include <utils/colored_io.hpp>
#include <utils/string_utils.hpp>

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
    if (!str.empty()) {
        tokens.push_back(str.substr(pos));  // Add the last token
    }
    return tokens;
}

std::vector<std::string> split(const std::string& str, const std::vector<char>& delimiters) {
    std::vector<std::string> tokens;
    size_t pos = 0;
    size_t del_pos;
    while (pos < str.size()) {
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
    if (pos < str.size()) {
        tokens.push_back(str.substr(pos));  // Add the last token
    }
    return tokens;
}

std::vector<std::string> split_keep_delimiters(const std::string& str,
                                               const std::vector<char>& delimiters) {
    std::vector<std::string> tokens;
    size_t pos = 0;
    size_t del_pos;
    while (pos < str.size()) {
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
            tokens.push_back(std::string(1, found_delim));  // Add the delimiter as a separate token
            pos = del_pos + 1;                              // Move past the delimiter
        } else {
            break;  // No more delimiters found
        }
    }
    if (pos < str.size()) {
        tokens.push_back(str.substr(pos));  // Add the last token
    }
    return tokens;
}

bool is_alphabetic(const std::string& str) {
    for (char c : str) {
        if (!is_alphabetic(c)) {
            return false;
        }
    }
    return true;
}

bool is_alphabetic(char c) { return std::isalpha(static_cast<unsigned char>(c)); }

bool is_numeric(const std::string& str) {
    for (char c : str) {
        if (!is_numeric(c)) {
            return false;
        }
    }
    return true;
}

bool is_numeric(char c) { return std::isdigit(static_cast<unsigned char>(c)); }

std::size_t get_length_without_color(const std::string& str) {
    char delim = '\033';

    std::size_t size = 0;
    for (std::size_t i = 0; i < str.size(); ++i) {
        if (str[i] == delim) {
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
