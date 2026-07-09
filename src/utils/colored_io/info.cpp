#include <utils/colored_io.hpp>

/**
 * @brief Prints a user-supplied informational message to the terminal.
 *
 * This utility binary is invoked by `scripts/install.sh` (and similar shell
 * scripts) to emit a formatted info line during execution.  It accepts
 * exactly one message argument, passes it to the shared INFO() printer
 * (which applies the configured color/style), and exits.
 *
 * @param argc  Argument count; expected to be exactly 2 (program name +
 *              message).  Fewer than 2 arguments is an error.
 * @param argv  Argument vector.  argv[0] is the program name (used only
 *              in the error message).  argv[1] is the message to print.
 *
 * @return 0 on success, 1 on usage error.
 *
 * @note The INFO() function writes to stderr and appends a newline.
 * @note This program terminates via ERROR() if insufficient arguments are
 *       provided -- ERROR() calls exit(EXIT_FAILURE), so the `return 1`
 *       immediately following it is unreachable but kept for clarity.
 */
int main(int argc, char* argv[]) {
    if (argc < 2) {
        ERROR("Usage: " + std::string(argv[0]) + " <message>");
        return 1;
    }

    std::string message = argv[1];
    INFO(message);

    return 0;
}
