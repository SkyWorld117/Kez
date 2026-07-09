#include <utils/colored_io.hpp>

/**
 * @brief Standalone binary that prints a success message.
 *
 * Invoked by scripts/install.sh to display consistently-coloured success output.
 * Accepts a single message argument and forwards it to the SUCCESS() macro.
 * If called without arguments, a usage hint is printed and the program exits.
 */
int main(int argc, char* argv[]) {
    if (argc < 2) {
        ERROR("Usage: " + std::string(argv[0]) + " <message>");
        return 1;
    }

    std::string message = argv[1];
    SUCCESS(message);

    return 0;
}
