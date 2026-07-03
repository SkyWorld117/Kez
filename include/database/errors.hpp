#pragma once

#include <string>

[[noreturn]] void raise_database_error(const std::string& message);
[[noreturn]] void raise_config_error(const std::string& message);
void emit_config_warning(const std::string& message);
