#include <utils/colored_io.hpp>

/**
 * @brief Standalone utility binary that prints a formatted error message via the
 *        ERROR() macro and terminates.
 *
 * This binary is invoked by the install scripts (e.g. scripts/install.sh) to
 * emit a consistently coloured, prefixed error message to stderr.  It accepts
 * exactly one argument: the error text to display.
 *
 * The ERROR() macro (from colored_io.hpp) prints the message and then calls
 * exit(EXIT_FAILURE), so the return 1 on line 12 is never reached in practice;
 * it serves only as a safety net should ERROR() ever change its behaviour.
 *
 * @param argc  Argument count (must be >= 2, i.e. at least one message token).
 * @param argv  Argument vector; argv[1] is the error message to print.
 *
 * @return 1 if called without arguments (usage hint shown first), otherwise
 *         the program never returns because ERROR() terminates the process.
 */
int main(int argc, char* argv[]) {
    if (argc < 2) {
        ERROR("Usage: " + std::string(argv[0]) + " <message>");
        return 1;
    }

    std::string message = argv[1];
    ERROR(message);

    return 0;
}
