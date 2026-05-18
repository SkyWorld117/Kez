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
