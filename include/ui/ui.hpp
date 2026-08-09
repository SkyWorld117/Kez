#pragma once

/**
 * @brief  Main entry point for the Kez command-line interface.
 *
 * Parses the first argument of `argv` as a command name and dispatches to the
 * appropriate handler function (`execute_init`, `execute_install`, etc.).
 * Reserved flags `-h` / `--help` print usage information and return without
 * dispatching; `-V` / `--version` print the version string and return.
 *
 * If the command is empty (i.e. `argc <= 1`) or is an unrecognised string,
 * usage information is printed and the program either returns (empty case) or
 * terminates with a non-zero exit code (unknown command case).
 *
 * @param argc  Argument count, as received from `main()`.
 * @param argv  Argument vector, as received from `main()`.
 *              `argv[0]` is the program name (ignored),
 *              `argv[1]` is the command name,
 *              `argv[2..]` are forwarded to the command handler.
 *
 * @see  CommandArguments (std::vector<std::string>) in commands.hpp
 * @see  execute_init, execute_install, execute_info, execute_dbcheck,
 *       execute_update, execute_utilities, execute_uconf,
 *       execute_environment, execute_compiler, execute_mpi, execute_vendor,
 *       execute_factory
 *
 * @note  This function does **not** return when an unknown command is given;
 *        it calls ERROR() and exit(EXIT_FAILURE) instead.
 */
void run_ui(int argc, char* argv[]);
