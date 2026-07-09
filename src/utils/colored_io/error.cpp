#include <utils/colored_io.hpp>

/**
 * @brief Standalone binary that prints a formatted error message and exits.
 *
 * Invoked by scripts/install.sh to display consistently-coloured error output.
 * Accepts a single message argument, forwards it to the ERROR() macro, and
 * terminates.  If called without arguments, a usage hint is printed first.
 *
 * @warning ERROR() itself calls exit(EXIT_FAILURE), so the return 1 after
 *          the macro call is never reached but kept as a safety guard.
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
