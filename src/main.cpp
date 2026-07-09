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

int main(int argc, char* argv[]) {
    run_ui(argc, argv);
    return 0;
}
