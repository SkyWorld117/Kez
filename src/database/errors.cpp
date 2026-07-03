#include <database/database.hpp>
#include <database/errors.hpp>
#include <utils/colored_io.hpp>

[[noreturn]] void raise_database_error(const std::string& message) {
    ERROR(message);
    throw DatabaseError(message);
}

[[noreturn]] void raise_config_error(const std::string& message) {
    ERROR(message);
    throw ConfigError(message);
}

void emit_config_warning(const std::string& message) { WARNING(message); }
