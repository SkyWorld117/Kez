#pragma once

#include <string>

std::string get_env_var(const std::string& var_name);
std::string get_env_var(const std::string& var_name, const std::string& err_msg);

std::string get_env_var_noerr(const std::string& var_name);
std::string get_env_var_noerr(const std::string& var_name, const std::string& default_value);