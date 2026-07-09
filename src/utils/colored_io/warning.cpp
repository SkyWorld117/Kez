#include <utils/colored_io.hpp>

/**
 * @brief Standalone executable that prints a warning message to the terminal.
 *
 * This binary is one of several small print helpers (print_info, print_warning,
 * print_error, print_success) invoked by scripts/install.sh to output
 * formatted messages. Each is a separate process so the shell script can
 * display colored output without embedding terminal logic in bash.
 *
 * The message is forwarded to the WARNING() macro from colored_io.hpp, which
 * prefixes it with a colored "[WARNING]" tag and writes it to stderr.
 *
 * @param argc  Argument count (must be >= 2).
 * @param argv  Argument vector; argv[1] is the message string to display.
 *
 * @return 0 on success, 1 if no message argument was provided.
 *
 * @note Terminates with exit code 1 via ERROR() if called with fewer than
 *       2 arguments. This is not a silent failure -- ERROR() prints the
 *       usage hint and calls exit(EXIT_FAILURE).
 */
int main(int argc, char* argv[]) {
    if (argc < 2) {
        ERROR("Usage: " + std::string(argv[0]) + " <message>");
        return 1;
    }

    std::string message = argv[1];
    WARNING(message);

    return 0;
}
