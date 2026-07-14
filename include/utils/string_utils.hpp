#pragma once

#include <string>
#include <vector>

/**
 * @brief Splits a string by a single delimiter character.
 *
 * Iterates through @p str and partitions it at every occurrence of @p delimiter.
 * Consecutive delimiters produce empty strings in the result. If @p str is
 * empty, an empty vector is returned.
 *
 * @param str       The input string to split.
 * @param delimiter The character at which to split.
 * @return A vector of substrings between occurrences of @p delimiter.
 *
 * @see split(const std::string&, const std::vector<char>&)
 * @see split_keep_delimiters(const std::string&, const std::vector<char>&)
 */
std::vector<std::string> split(const std::string& str, char delimiter);

/**
 * @brief Splits a string by multiple delimiter characters.
 *
 * Any character in @p delimiters acts as a split point. Consecutive delimiters
 * (including mixed ones) produce empty strings in the result. If @p str is
 * empty, an empty vector is returned.
 *
 * @param str        The input string to split.
 * @param delimiters The set of characters that each serve as a split point.
 * @return A vector of substrings separated by any of the given delimiters.
 *
 * @see split(const std::string&, char)
 * @see split_keep_delimiters(const std::string&, const std::vector<char>&)
 */
std::vector<std::string> split(const std::string& str, const std::vector<char>& delimiters);

/**
 * @brief Splits a string by multiple delimiter characters, retaining the
 *        delimiter characters in the output.
 *
 * Behaves like split(const std::string&, const std::vector<char>&), except
 * each delimiter character is preserved as its own single-character element in
 * the result. This is useful when the delimiter carries semantic value (e.g.
 * operators in an expression).
 *
 * @param str        The input string to split.
 * @param delimiters The set of delimiter characters to split on and retain.
 * @return A vector of substrings and single-character delimiter tokens.
 *
 * @see split(const std::string&, const std::vector<char>&)
 */
std::vector<std::string> split_keep_delimiters(const std::string& str,
                                               const std::vector<char>& delimiters);

/**
 * @brief Checks whether every character in a string is alphabetic.
 *
 * Returns false for an empty string. Uses std::isalpha on each character.
 *
 * @param str The string to test.
 * @return true if @p str is non-empty and all characters are alphabetic;
 *         false otherwise.
 *
 * @see is_alphabetic(char)
 */
bool is_alphabetic(const std::string& str);

/**
 * @brief Checks whether a single character is alphabetic (a-z, A-Z).
 *
 * @param c The character to test.
 * @return true if @p c is an alphabetic letter; false otherwise.
 *
 * @see is_alphabetic(const std::string&)
 */
bool is_alphabetic(char c);

/**
 * @brief Checks whether every character in a string is numeric.
 *
 * Returns false for an empty string. Uses std::isdigit on each character.
 *
 * @param str The string to test.
 * @return true if @p str is non-empty and all characters are digits;
 *         false otherwise.
 *
 * @see is_numeric(char)
 */
bool is_numeric(const std::string& str);

/**
 * @brief Checks whether a single character is a decimal digit (0-9).
 *
 * @param c The character to test.
 * @return true if @p c is a digit; false otherwise.
 *
 * @see is_numeric(const std::string&)
 */
bool is_numeric(char c);

/**
 * @brief Returns the visible length of a string after stripping ANSI color
 *        escape sequences.
 *
 * Removes sequences matching the pattern "\033[...m" (CSI SGR codes) before
 * computing the character count. Non-visible CSI sequences that do not end
 * with 'm' are not stripped.
 *
 * @param str The input string, possibly containing ANSI color codes.
 * @return The number of visible characters in @p str.
 */
int get_length_without_color(const std::string& str);

/**
 * @brief Compares two version strings using numeric segment comparison.
 *
 * Splits both strings on '.' and compares corresponding segments as integers.
 * The first non-equal segment determines the result. If all common segments
 * are equal, the string with fewer segments is considered smaller.
 *
 * @param left  The first version string (e.g. "1.2.3").
 * @param right The second version string (e.g. "1.10.0").
 * @return A negative integer if @p left < @p right,
 *         zero               if @p left == @p right,
 *         a positive integer if @p left > @p right.
 */
int compare_versions(const std::string& left, const std::string& right);

/**
 * @brief Removes leading and trailing whitespace from a string.
 *
 * Whitespace is defined by std::isspace (spaces, tabs, newlines, etc.).
 * The original string is not modified; a trimmed copy is returned.
 *
 * @param input The string to trim.
 * @return A new string with leading and trailing whitespace removed.
 */
std::string trim(const std::string& input);

/**
 * @brief Appends a value to a vector only if it is not already present.
 *
 * Performs a linear search over @p values. If @p value is not found, it is
 * push_back'd. If it is already present, the vector is left unchanged.
 *
 * @param values The destination vector (modified in-place).
 * @param value  The string to conditionally append.
 */
void append_unique(std::vector<std::string>& values, const std::string& value);

/**
 * @brief Concatenates a vector of strings into a single string, separated by
 *        a given delimiter.
 *
 * If the vector is empty, an empty string is returned. The separator is
 * inserted between elements but not appended after the last element.
 *
 * @param values    The strings to join.
 * @param separator The delimiter placed between elements (default: a single
 *                  space).
 * @return The concatenated string.
 *
 * @see split(const std::string&, char)
 */
std::string join(const std::vector<std::string>& values, const std::string& separator = " ");

/**
 * @brief Checks whether a string is a valid shell variable assignment name.
 *
 * A valid shell assignment name consists only of uppercase letters, digits,
 * and underscores, and must not begin with a digit. This is used to
 * distinguish environment-variable-style tokens from other identifiers.
 *
 * @param name The string to test.
 * @return true if @p name conforms to the shell assignment naming rules;
 *         false otherwise.
 */
bool is_shell_assignment(const std::string& name);
