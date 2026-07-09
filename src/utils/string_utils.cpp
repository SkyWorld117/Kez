#include <algorithm>
#include <cctype>
#include <string>
#include <utils/colored_io.hpp>
#include <utils/string_utils.hpp>

/**
 * @brief Splits a string by a single delimiter character, discarding empty
 *        tokens.
 *
 * Iterates through @p str and partitions it at every occurrence of @p delimiter.
 * Consecutive delimiters (or a leading/trailing delimiter) produce empty
 * substrings which are silently discarded.  An empty input string produces an
 * empty vector.
 *
 * @param str       The input string to split.
 * @param delimiter The character at which to split.
 * @return A vector of non-empty substrings between occurrences of
 *         @p delimiter.
 */
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

/**
 * @brief Splits a string by multiple possible delimiter characters, discarding
 *        empty tokens.
 *
 * Any character present in @p delimiters acts as a split point.  Among the
 * delimiters the nearest one (lowest index) in the remaining substring is
 * chosen at each step.  Empty tokens arising from consecutive or leading
 * delimiters are silently discarded.  An empty input string produces an empty
 * vector.
 *
 * @param str        The input string to split.
 * @param delimiters The set of characters, each of which serves as a split
 *                   point.
 * @return A vector of non-empty substrings separated by any of the given
 *         delimiters.
 */
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

/**
 * @brief Splits a string by multiple delimiter characters, retaining each
 *        delimiter as its own token in the output.
 *
 * Behaves like split(const std::string&, const std::vector<char>&), except
 * each delimiter character is preserved as a single-character element in the
 * result vector.  Empty tokens between consecutive delimiters are still
 * discarded.  This is useful when the delimiter carries semantic value (e.g.
 * operators in an expression string).
 *
 * @param str        The input string to split.
 * @param delimiters The set of delimiter characters to split on and retain.
 * @return A vector of non-empty substrings interleaved with single-character
 *         delimiter tokens.
 */
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

/**
 * @brief Checks whether every character in a string is alphabetic (a-z, A-Z).
 *
 * Returns false for an empty string.  Each character is tested via
 * is_alphabetic(char), which delegates to std::isalpha.  This function does
 * **not** terminate the program on failure.
 *
 * @param str The string to test.
 * @return true if @p str is non-empty and every character is an alphabetic
 *         letter; false otherwise.
 */
bool is_alphabetic(const std::string& str) {
    for (char c : str) {
        if (!is_alphabetic(c)) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Checks whether a single character is an alphabetic letter (a-z, A-Z).
 *
 * Delegates to std::isalpha with an unsigned char cast to avoid undefined
 * behaviour on negative char values (common with extended characters).
 *
 * @param c The character to test.
 * @return true if @p c is an alphabetic letter; false otherwise.
 */
bool is_alphabetic(char c) { return std::isalpha(static_cast<unsigned char>(c)); }

/**
 * @brief Checks whether every character in a string is a decimal digit (0-9).
 *
 * Returns false for an empty string.  Each character is tested via
 * is_numeric(char), which delegates to std::isdigit.
 *
 * @param str The string to test.
 * @return true if @p str is non-empty and every character is a digit; false
 *         otherwise.
 */
bool is_numeric(const std::string& str) {
    for (char c : str) {
        if (!is_numeric(c)) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Checks whether a single character is a decimal digit (0-9).
 *
 * Delegates to std::isdigit with an unsigned char cast to avoid undefined
 * behaviour on negative char values.
 *
 * @param c The character to test.
 * @return true if @p c is a decimal digit; false otherwise.
 */
bool is_numeric(char c) { return std::isdigit(static_cast<unsigned char>(c)); }

/**
 * @brief Returns the visible (display) length of a string after stripping
 *        ANSI CSI SGR colour escape sequences.
 *
 * Scans the input character-by-character.  When a sequence starting with
 * \033 (ESC) is encountered, characters up to and including the terminating
 * 'm' are skipped and not counted.  All other characters increment the
 * returned length.  This handles standard colour/reset codes
 * (e.g. "\033[31m") but not other CSI sequences that end with a different
 * final byte.
 *
 * @param str The input string, possibly containing ANSI escape codes.
 * @return The number of visible glyphs (non-escape) in @p str.
 */
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

/**
 * @brief Compares two version strings using numeric-and-alphabetic segment
 *        comparison.
 *
 * The comparison proceeds in two phases:
 *   1. Split each version on '.' and compare corresponding top-level segments
 *      as strings.
 *   2. When two top-level segments differ, further split each on alphabetic
 *      character boundaries (e.g. "12a3" -> ["12", "a", "3"]) using
 *      split_keep_delimiters.  Numeric sub-segments are compared
 *      numerically via std::stoll; alphabetic sub-segments are compared
 *      lexicographically.
 *
 * The first non-equal segment determines the result.  If all common segments
 * are equal, the version with fewer segments is considered smaller.
 *
 * @warning Calls std::stoll, which throws std::invalid_argument or
 *          std::out_of_range on malformed numeric sub-segments.  The caller
 *          should ensure the input is well-formed; no ERROR/exit is called
 *          here.
 *
 * @param left  The first version string (e.g. "1.2.3", "1.2a1").
 * @param right The second version string (e.g. "1.10.0").
 * @return A negative integer if @p left < @p right,
 *         zero               if @p left == @p right,
 *         a positive integer if @p left > @p right.
 */
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

/**
 * @brief Removes leading and trailing whitespace from a string.
 *
 * Whitespace is defined as space (' '), newline ('\n'), carriage return ('\r'),
 * and tab ('\t').  The function searches for the first and last characters
 * that are **not** whitespace and returns the substring between them.  If the
 * input is entirely whitespace, an empty string is returned.
 *
 * @param input The string to trim.
 * @return A new string with leading and trailing whitespace removed, or an
 *         empty string if @p input consists entirely of whitespace.
 */
std::string trim(const std::string& input) {
    const std::size_t first = input.find_first_not_of(" \n\r\t");
    if (first == std::string::npos) {
        return {};
    }
    const std::size_t last = input.find_last_not_of(" \n\r\t");
    return input.substr(first, last - first + 1);
}

/**
 * @brief Appends a value to a vector only if it is non-empty and not already
 *        present.
 *
 * Performs a linear search (std::find) over @p values to check for
 * duplicates.  If @p value is empty no action is taken.  This function
 * modifies @p values in-place.
 *
 * @param values The destination vector (modified in-place).
 * @param value  The string to conditionally append.
 */
void append_unique(std::vector<std::string>& values, const std::string& value) {
    if (!value.empty() && std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

/**
 * @brief Concatenates a vector of strings into a single string with a given
 *        separator inserted between elements.
 *
 * An empty vector produces an empty string.  The separator is placed between
 * elements but not appended after the final element.
 *
 * @param values    The strings to join.
 * @param separator The delimiter placed between elements.
 * @return The concatenated string.
 */
std::string join(const std::vector<std::string>& values, const std::string& separator) {
    std::string result;
    for (const std::string& value : values) {
        result += (result.empty() ? "" : separator) + value;
    }
    return result;
}

/**
 * @brief Checks whether a string is a valid shell variable assignment name.
 *
 * A valid shell variable name (per POSIX convention for environment variables)
 * must:
 *   - Be non-empty.
 *   - Start with an underscore ('_') or an uppercase letter.
 *   - Contain only underscores, uppercase letters, and digits.
 *
 * Lowercase letters, hyphens, and other characters are not permitted.  This
 * is used to distinguish environment-variable-style tokens from other
 * identifiers in the codebase.
 *
 * @param name The string to test.
 * @return true if @p name conforms to the shell assignment naming rules;
 *         false otherwise.
 */
bool is_shell_assignment(const std::string& name) {
    if (name.empty() || (name[0] != '_' && !std::isupper(static_cast<unsigned char>(name[0])))) {
        return false;
    }
    return std::all_of(name.begin() + 1, name.end(), [](const char character) {
        return character == '_' || std::isupper(static_cast<unsigned char>(character)) ||
               std::isdigit(static_cast<unsigned char>(character));
    });
}
