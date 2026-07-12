/**
 * @file print_message.cpp
 * @brief Standalone binary that prints a formatted, colored message and exits.
 *
 * Invoked by shell scripts (install.sh, init.sh, tools scripts) to display
 * consistently-coloured output.  Accepts a message type and a message text,
 * then prints using the corresponding INFO/WARNING/ERROR/SUCCESS macro.
 *
 * Usage: print_message <info|warning|error|success> <message>
 *
 * When the type is "error", output goes to stderr; all other types go to
 * stdout.  If the arguments are missing, a usage hint is printed and the
 * program exits with a non-zero status.
 */

#include <cstdlib>
#include <iostream>
#include <string>
#include <utils/colored_io.hpp>

/**
 * @brief Print a usage hint to stderr and exit with failure.
 */
[[noreturn]] static void print_usage(const char* prog_name) {
    std::cerr << "[E]: Usage: " << prog_name << " <info|warning|error|success> <message>"
              << std::endl;
    exit(EXIT_FAILURE);
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        print_usage(argv[0]);
    }

    const std::string type    = argv[1];
    const std::string message = argv[2];

    if (type == "info") {
        INFO(message);
    } else if (type == "warning") {
        WARNING(message);
    } else if (type == "error") {
        ERROR(message);
    } else if (type == "success") {
        SUCCESS(message);
    } else {
        ERROR("Unknown message type: " + type);
        print_usage(argv[0]);
    }

    return 0;
}
