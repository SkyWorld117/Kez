#include <cstdlib>
#include <string>
#include <utils/bash_utils.hpp>
#include <utils/colored_io.hpp>

/** @brief Retrieves a required environment variable, building a default error message on failure. */
std::string get_env_var(const std::string& var_name) {
    std::string err_msg = "Environment variable '" + var_name + "' is not set.";
    return get_env_var(var_name, err_msg);
}

/** @brief Retrieves a required environment variable, terminating with a caller-supplied error message if unset. */
std::string get_env_var(const std::string& var_name, const std::string& err_msg) {
    const char* value = std::getenv(var_name.c_str());
    if (!value) {
        ERROR(err_msg);
        exit(EXIT_FAILURE);
    }
    return std::string(value);
}

/** @brief Retrieves an optional environment variable, returning empty string when unset. */
std::string get_env_var_noerr(const std::string& var_name) {
    return get_env_var_noerr(var_name, "");
}

/** @brief Retrieves an optional environment variable, returning a caller-supplied default when unset. */
std::string get_env_var_noerr(const std::string& var_name, const std::string& default_value) {
    const char* value = std::getenv(var_name.c_str());
    if (!value) {
        return default_value;
    }
    return std::string(value);
}

/** @brief Wraps a string in single quotes, escaping embedded single quotes for safe shell use. */
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

/** @brief Wraps a string in double quotes, escaping embedded backslashes and double-quotes for safe shell use. */
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
