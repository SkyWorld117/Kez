/**
 * @file main.cpp
 * @brief Entry point for the Kez package manager CLI.
 *
 * Delegates all user interaction and command dispatch to the UI module.
 * This file exists solely to instantiate the application binary; no
 * argument validation, configuration loading, or error handling occurs
 * here -- those responsibilities belong to the components invoked by
 * run_ui().
 */

#include <ui/ui.hpp>

/**
 * @brief Program entry point.
 *
 * Forwards the raw command-line arguments to the interactive UI dispatcher,
 * which parses subcommands (install, init, packages, environment, factory,
 * etc.), orchestrates the backend pipeline, and produces the shell-level
 * install plan or output.
 *
 * This function never returns directly except through the normal exit path
 * shown below. However, the call chain rooted at run_ui() may terminate
 * the process early if an irrecoverable error is encountered (via ERROR()
 * which prints the diagnostic and calls exit(EXIT_FAILURE)), or if the
 * user requests `--help` or `--version` (which may call exit(EXIT_SUCCESS)
 * after printing).
 *
 * @param argc Number of command-line arguments (including the program name).
 * @param argv Array of C-style argument strings; argv[0] is the program
 *             name, argv[1..argc-1] are subcommand and option tokens.
 * @return int 0 on clean exit, though in practice the process may already
 *             have been terminated by a callee before this return executes.
 */
int main(int argc, char* argv[]) {
    run_ui(argc, argv);
    return 0;
}
