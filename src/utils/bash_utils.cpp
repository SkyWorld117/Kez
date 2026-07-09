#include <cstdlib>
#include <string>
#include <utils/bash_utils.hpp>
#include <utils/colored_io.hpp>

/**
 * @brief Retrieves the value of an environment variable; terminates on failure.
 *
 * Convenience overload that delegates to the two-parameter version with an
 * auto-generated error message of the form
 * "Environment variable '<var_name>' is not set."
 *
 * @param var_name  The name of the environment variable to read.
 *
 * @return The value of the environment variable as a std::string.
 *
 * @warning Terminates the process (exit(EXIT_FAILURE)) if the variable is not
 *          set.
 *
 * @see get_env_var(const std::string&, const std::string&)
 */
std::string get_env_var(const std::string& var_name) {
    std::string err_msg = "Environment variable '" + var_name + "' is not set.";
    return get_env_var(var_name, err_msg);
}

/**
 * @brief Retrieves the value of an environment variable; terminates on failure
 *        with a custom error message.
 *
 * Calls std::getenv() to look up the variable. If the variable is not set, the
 * provided error message is printed via the ERROR() macro and the program
 * terminates with exit(EXIT_FAILURE).
 *
 * @param var_name  The name of the environment variable to read.
 * @param err_msg   Custom error message printed before termination when the
 *                  variable is missing.
 *
 * @return The value of the environment variable as a std::string.
 *
 * @warning Terminates the process (exit(EXIT_FAILURE)) if the variable is not
 *          set.
 */
std::string get_env_var(const std::string& var_name, const std::string& err_msg) {
    const char* value = std::getenv(var_name.c_str());
    if (!value) {
        ERROR(err_msg);
        exit(EXIT_FAILURE);
    }
    return std::string(value);
}

/**
 * @brief Retrieves the value of an environment variable without terminating on
 *        failure.
 *
 * Convenience overload that delegates to the two-parameter version with an
 * empty default value. If the variable is not set, an empty string is returned
 * instead of terminating.
 *
 * @param var_name  The name of the environment variable to read.
 *
 * @return The value of the environment variable, or an empty string if the
 *         variable is not set.
 *
 * @see get_env_var_noerr(const std::string&, const std::string&)
 */
std::string get_env_var_noerr(const std::string& var_name) {
    return get_env_var_noerr(var_name, "");
}

/**
 * @brief Retrieves the value of an environment variable with a fallback
 *        default.
 *
 * Calls std::getenv() to look up the variable. If the variable is not set, the
 * supplied default_value is returned. Unlike get_env_var(), this function never
 * terminates the process.
 *
 * @param var_name       The name of the environment variable to read.
 * @param default_value  The value to return when the variable is unset.
 *
 * @return The value of the environment variable, or default_value if the
 *         variable is not set.
 */
std::string get_env_var_noerr(const std::string& var_name, const std::string& default_value) {
    const char* value = std::getenv(var_name.c_str());
    if (!value) {
        return default_value;
    }
    return std::string(value);
}

/**
 * @brief Quotes a string for safe use as a single-quoted shell argument.
 *
 * Wraps the input in single quotes and escapes any embedded single-quote
 * characters by replacing them with the sequence '\'' (end-quote, escaped
 * literal quote, re-open-quote). This guarantees that the output, when
 * evaluated by a Bourne-compatible shell, reproduces the original string
 * exactly.
 *
 * Example:
 *   shell_single_quote("it's done")  ->  "'it'\\''s done'"
 *
 * @param value  The raw string to quote.
 *
 * @return A single-quoted string safe for shell eval.
 */
std::string shell_single_quote(const std::string& value) {
    std::string result = "'";
    for (const char character : value) {
        if (character == '\'') {
            result += "'\\''";
        } else {
            result += character;
        }
    }
    return result + "'";
}

/**
 * @brief Quotes a string for safe use as a double-quoted shell argument.
 *
 * Wraps the input in double quotes and escapes any backslash or double-quote
 * characters by prefixing them with a backslash. This ensures that the output,
 * when evaluated by a Bourne-compatible shell, reproduces the original string
 * exactly. Other characters (dollar signs, backticks, exclamation marks) are
 * **not** escaped, so the caller must ensure the input contains no characters
 * that would be interpreted inside double quotes, or use shell_single_quote()
 * instead.
 *
 * Example:
 *   shell_double_quote("hello \"world\"")  ->  "\"hello \\\"world\\\"\""
 *
 * @param value  The raw string to quote.
 *
 * @return A double-quoted string safe for shell eval in contexts where
 *         variable expansion is desired.
 *
 * @warning Dollar signs ($), backticks (`), and exclamation marks (!) in the
 *          input are **not** escaped and will be interpreted by the shell. Use
 *          shell_single_quote() for arbitrary or untrusted input.
 *
 * @see shell_single_quote  Prefer this for arbitrary or untrusted strings.
 */
std::string shell_double_quote(const std::string& value) {
    std::string result = "\"";
    for (const char character : value) {
        if (character == '\\' || character == '\"') {
            result += '\\';
        }
        result += character;
    }
    return result + "\"";
}
