#include <utils/colored_io.hpp>

/**
 * @brief Entry point for the success-message printing utility.
 *
 * Reads a message string from the command-line arguments and prints it to
 * standard output using the SUCCESS() helper macro (typically rendered in
 * green by the terminal).  This binary is used by the install scripts to
 * display final confirmation that a build step completed successfully.
 *
 * @param argc  Argument count (must be at least 2; argv[0] is the program
 *              name and argv[1] is the message to display).
 * @param argv  Argument vector.  argv[1] is interpreted as the success
 *              message.  Any further arguments are silently ignored.
 *
 * @return 0 on success, 1 on error.
 *
 * @warning If fewer than two arguments are provided, an error message is
 *          printed via the ERROR() macro and the program exits with a
 *          non-zero return code (1).  This does NOT call exit() directly
 *          but relies on the caller (or the ERROR() macro itself) to
 *          terminate the process.
 *
 * @note This program does not attempt to sanitize or escape the message;
 *      it is the caller's responsibility to supply a safe, printable string.
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
