#include <utils/string_utils.hpp>

#include <fstream>
#include <sstream>

std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    for (char ch : str) {
        if (ch == delimiter) {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
        } else {
            token += ch;
        }
    }
    if (!token.empty()) {
        tokens.push_back(token);
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

std::string read_file(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs) {
        return "";
    }
    std::ostringstream buffer;
    buffer << ifs.rdbuf();
    return buffer.str();
}
