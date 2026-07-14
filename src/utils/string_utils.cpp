#include <algorithm>
#include <cctype>
#include <charconv>
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
    // Add the remainder after the last delimiter (if non-empty)
    if (!str.empty()) {
        tokens.push_back(str.substr(pos));
    }
    return tokens;
}

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

int compare_versions(const std::string& left, const std::string& right) {
    // Allocation-free version comparator.
    //
    // Walks both version strings as dot-separated segments without creating
    // intermediate vectors or substrings.  Each segment is further split into
    // numeric (digit) and alphabetic (lowercase-letter) sub-parts on the fly.
    //
    // Numeric sub-parts are compared as integers via std::from_chars (no
    // exceptions).  Alphabetic / mixed sub-parts are compared character by
    // character.  If all sub-parts in a segment are equal, the segment with
    // fewer sub-parts is considered smaller.  If all segments are equal, the
    // version with fewer segments is considered smaller.
    const char* l     = left.data();
    const char* l_end = left.data() + left.size();
    const char* r     = right.data();
    const char* r_end = right.data() + right.size();

    while (l < l_end && r < r_end) {
        // ── Compare one dot-separated segment ──────────────────────────
        const char* const l_seg = l;
        const char* const r_seg = r;

        // Advance to the next dot or end-of-string to find segment boundaries.
        while (l < l_end && *l != '.') ++l;
        while (r < r_end && *r != '.') ++r;

        // Compare sub-parts within the segment.
        const char* ls = l_seg;
        const char* rs = r_seg;
        while (ls < l && rs < r) {
            if (std::isdigit(static_cast<unsigned char>(*ls)) &&
                std::isdigit(static_cast<unsigned char>(*rs))) {
                // Both start with a number → compare numerically.
                long long lv, rv;
                const auto lres = std::from_chars(ls, l, lv);
                const auto rres = std::from_chars(rs, r, rv);
                // from_chars always succeeds for digit-only input.
                if (lv != rv) {
                    return lv < rv ? -1 : 1;
                }
                ls = lres.ptr;
                rs = rres.ptr;
            } else {
                // At least one side is non-numeric → compare character by character.
                if (*ls != *rs) {
                    return *ls < *rs ? -1 : 1;
                }
                ++ls;
                ++rs;
            }
        }

        // One side ran out of sub-parts within this segment.
        if (ls < l || rs < r) {
            return ls < l ? 1 : -1;
        }

        // Both sub-parts exhausted; advance past the dot (if any) and continue.
        if (l < l_end) ++l;  // skip '.'
        if (r < r_end) ++r;  // skip '.'
    }

    // All common segments are equal.  The version with more segments is larger.
    if (l < l_end || r < r_end) {
        return l < l_end ? 1 : -1;
    }
    return 0;
}

std::string trim(const std::string& input) {
    const std::size_t first = input.find_first_not_of(" \n\r\t");
    if (first == std::string::npos) {
        return {};
    }
    const std::size_t last = input.find_last_not_of(" \n\r\t");
    return input.substr(first, last - first + 1);
}

void append_unique(std::vector<std::string>& values, const std::string& value) {
    if (!value.empty() && std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

std::string join(const std::vector<std::string>& values, const std::string& separator) {
    std::string result;
    for (const std::string& value : values) {
        result += (result.empty() ? "" : separator) + value;
    }
    return result;
}

bool is_shell_assignment(const std::string& name) {
    if (name.empty() || (name[0] != '_' && !std::isupper(static_cast<unsigned char>(name[0])))) {
        return false;
    }
    return std::all_of(name.begin() + 1, name.end(), [](const char character) {
        return character == '_' || std::isupper(static_cast<unsigned char>(character)) ||
               std::isdigit(static_cast<unsigned char>(character));
    });
}
