#include <string>
#include <utils/bash_utils.hpp>
#include <utils/colored_io.hpp>

std::string get_env_var(const std::string& var_name) {
    std::string err_msg = "Environment variable '" + var_name + "' is not set.";
    return get_env_var(var_name, err_msg);
}

std::string get_env_var(const std::string& var_name, const std::string& err_msg) {
    const char* value = std::getenv(var_name.c_str());
    if (!value) {
        ERROR(err_msg);
        exit(EXIT_FAILURE);
    }
    return std::string(value);
}

std::string get_env_var_noerr(const std::string& var_name) {
    return get_env_var_noerr(var_name, "");
}

std::string get_env_var_noerr(const std::string& var_name, const std::string& default_value) {
    const char* value = std::getenv(var_name.c_str());
    if (!value) {
        return default_value;
    }
    return std::string(value);
}