# Kez

## Project Overview

Kez is an HPC-focused package manager for GNU/Linux systems. The logic is as follows:
1. Parse the package metadata from the database.
2. Resolve dependencies based on the parsed metadata.
3. Generate a user configurable yaml file based on the resolved dependencies.
4. Parse the yaml file to generate bash commands for installation.
5. Execute the generated bash commands to install the packages.

Kez uses a C++ backend for performance and a bash frontend for environment manipulation.

## Coding Style

The C++ backend should be as efficient as possible, so unless there is a good reason, avoid using complex data structures such as classes and keep a function-heavy C-style programming approach. It is acceptable to use C++ built-in data structures such as `std::vector` and `std::map`.

Exceptions will never be tolerated or caught. If anything goes wrong, the program should terminate immediately with a non-zero exit code. Use `ERROR(error_msg)` from `colored_io.hpp` to print the error message and terminate the program with `exit(EXIT_FAILURE)`.

Generic utility functions should be declared in `include/utils/` and defined in `src/utils/`. If a utility function is only used in one component, it should be placed in that component's directory or directly in the `.cpp` file.

It is unlikely that this project will be used as a library, so there is no need to create a public API. All header files should be placed in `include/` and all source files should be placed in `src/`. The directory structure should serve for organization and clarity rather than for library distribution.

After developing each component, it is important to write tests in `tests/` to ensure that the implementation is correct and robust.

There is no need to format the codebase. All the `.yaml` files and `.cpp`/`.hpp` files will be automatically formatted by `pre-commit` hooks.

