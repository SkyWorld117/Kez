#pragma once

#include <utils/colors.h>

#define INFO(message)    print_info(message)
#define DEBUG(message)   print_debug(message)
#define WARNING(message) print_warning(message)
#define ERROR(message)   print_error(message)
#define SUCCESS(message) print_success(message)

#include <unistd.h>  // for isatty and STDOUT_FILENO

#include <iostream>
#include <string>

inline void print_info(const std::string& message) {
    if (isatty(STDOUT_FILENO)) {
        std::cout << INFO_COLOR << "[I]: " << message << COLOR_RESET << std::endl;
    } else {
        std::cout << "[I]: " << message << std::endl;
    }
}

inline void print_debug(const std::string& message) {
#ifdef DDEBUG
    if (isatty(STDOUT_FILENO)) {
        std::cout << DEBUG_COLOR << "[D]: " << message << COLOR_RESET << std::endl;
    } else {
        std::cout << "[D]: " << message << std::endl;
    }
#endif // DEBUG
}

inline void print_warning(const std::string& message) {
    if (isatty(STDOUT_FILENO)) {
        std::cout << WARNING_COLOR << "[W]: " << message << COLOR_RESET << std::endl;
    } else {
        std::cout << "[W]: " << message << std::endl;
    }
}

inline void print_error(const std::string& message) {
    if (isatty(STDERR_FILENO)) {
        std::cerr << ERROR_COLOR << "[E]: " << message << COLOR_RESET << std::endl;
    } else {
        std::cerr << "[E]: " << message << std::endl;
    }
}

inline void print_success(const std::string& message) {
    if (isatty(STDOUT_FILENO)) {
        std::cout << SUCCESS_COLOR << "[S]: " << message << COLOR_RESET << std::endl;
    } else {
        std::cout << "[S]: " << message << std::endl;
    }
}
