#pragma once

#include <string>

/**
 * @brief Retrieves the value of an environment variable; terminates on failure.
 *
 * Calls the two-parameter overload with an auto-generated error message of the
 * form "Environment variable '<var_name>' is not set." If the variable is
 * unset, the program prints the error and terminates with exit(EXIT_FAILURE).
 *
 * @param var_name  The name of the environment variable to read.
 *
 * @return The value of the environment variable as a std::string.
 *
 * @warning Terminates the process (exit(EXIT_FAILURE)) if the variable is not
 *          set.
 *
 * @see get_env_var(const std::string&, const std::string&)  The overload this
 *      delegates to.
 */
std::string get_env_var(const std::string& var_name);

/**
 * @brief Retrieves the value of an environment variable; terminates on failure
 *        with a custom message.
 *
 * Calls std::getenv() to look up the variable.  If the variable is not set,
 * the provided error message is printed via the ERROR() macro and the program
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
std::string get_env_var(const std::string& var_name, const std::string& err_msg);

/**
 * @brief Retrieves the value of an environment variable without terminating on
 *        failure.
 *
 * Calls the two-parameter overload with an empty default value.  If the
 * variable is not set, an empty string is returned instead of terminating.
 *
 * @param var_name  The name of the environment variable to read.
 *
 * @return The value of the environment variable, or an empty string if the
 *         variable is not set.
 *
 * @see get_env_var_noerr(const std::string&, const std::string&)  The overload
 *      this delegates to.
 */
std::string get_env_var_noerr(const std::string& var_name);

/**
 * @brief Retrieves the value of an environment variable with a fallback
 *        default.
 *
 * Calls std::getenv() to look up the variable.  If the variable is not set,
 * the supplied default_value is returned.  Unlike get_env_var(), this function
 * never terminates the process.
 *
 * @param var_name       The name of the environment variable to read.
 * @param default_value  The value to return when the variable is unset.
 *
 * @return The value of the environment variable, or default_value if the
 *         variable is not set.
 */
std::string get_env_var_noerr(const std::string& var_name, const std::string& default_value);

/**
 * @brief Quotes a string for safe use as a single-quoted shell argument.
 *
 * Wraps the input in single quotes and escapes any embedded single-quote
 * characters by replacing them with the sequence '\'' (end-quote, escaped
 * literal quote, re-open-quote).  This guarantees that the output, when
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
std::string shell_single_quote(const std::string& value);

/**
 * @brief Quotes a string for safe use as a double-quoted shell argument.
 *
 * Wraps the input in double quotes and escapes any backslash or double-quote
 * characters by prefixing them with a backslash.  This ensures that the
 * output, when evaluated by a Bourne-compatible shell, reproduces the original
 * string exactly.  Other characters (dollar signs, backticks, exclamation
 * marks) are **not** escaped, so the caller must ensure the input contains no
 * characters that would be interpreted inside double quotes, or use
 * shell_single_quote() instead.
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
 *          input are **not** escaped and will be interpreted by the shell.
 *          Use shell_single_quote() for arbitrary / untrusted input.
 *
 * @see shell_single_quote  Prefer this for arbitrary or untrusted strings.
 */
std::string shell_double_quote(const std::string& value);
