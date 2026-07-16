# Repository Guidelines

## Project Structure & Module Organization

Kez combines a C++ backend, Bash frontend, and YAML package database. C++ headers live in `include/` and mirror implementations in `src/` by component (`database/`, `dependency_resolver/`, `uconf_generator/`, `ui/`, and `utils/`). GoogleTest cases are in `tests/`. Package recipes use `database/<package>/latest.yaml`; related source patches belong in `patches/<package>/`. Shell entry points and helpers are in `main.sh`, `scripts/`, and `tools/`. Keep user, developer, and packager documentation in `docs/`. Build artifacts under `bin/`, `lib/`, and `obj/` are generated and should not be committed.

## Build, Test, and Development Commands

Set a writable work directory and initialize the environment before building:

```bash
export KEZ_WORKDIR=/path/to/kez-workdir
source setup-env.sh
make -j                         # build the library and command-line tools
make test                       # build and run the full GoogleTest suite
./bin/kez_tests --gtest_filter=TemporaryDatabase.*  # run a focused test group
make clean                      # remove compiled outputs
pre-commit run --all-files      # format and validate C++ and YAML files
```

For recipe changes, run `kez dbcheck --only <package>` and generate a configuration with `kez uconf <package>`.

## Coding Style & Naming Conventions

Use C++17 and follow `.clang-format`: Google-derived style, four-space indentation, no tabs, and a 100-column limit. Name source and header files with `snake_case` and preserve the matching `include/<component>/` and `src/<component>/` layout. Put reusable helpers in `utils/`; keep component-specific helpers local. Prefer focused functions and existing error handling patterns. Recipe directories are lowercase, using hyphens for multiword package names (for example, `intel-oneapi-mkl`). Pre-commit applies `clang-format` and `yamlfmt`.

## Coding Philosophy

The C++ backend should be as efficient as possible. Unless there is a good reason (e.g., better performance, better simplicity), avoid using complex data structures such as classes and keep a function-heavy C-style programming approach. It is fine to use C++ built-in data structures. Enum classes are permitted.

Exceptions will never be tolerated or caught. If anything goes wrong, the program should terminate immediately with a non-zero exit code. Use `ERROR(error_msg)` from `colored_io.hpp` to print the error message and terminate the program with `exit(EXIT_FAILURE)`. Bash scripts should use `colored_io.sh` for output and terminate on errors.

Generic utility functions should be declared in `include/utils/` and defined in `src/utils/`. If a utility function is only used in one component, it should be placed in that component's directory or directly in the `.cpp` file.

## Testing Guidelines

Add or update `tests/<component>_test.cpp` for behavior changes. Use descriptive GoogleTest suite and case names, and cover success paths, invalid input, and dependency or filesystem edge cases. There is no stated numeric coverage threshold, but every pull request must pass `make test`. Package recipes should also be built and installed on a real HPC cluster when possible.

## Documentation Guidelines

The documentation is located in the `docs/` directory. It contains structured guides and references for both users and developers. 

Next to the documentation, it is also required to write doxygen-style comments in the codebase. This will allow for automatic generation of API documentation. Run `doxygen` at the root of the project to generate the documentation. DO NOT PUSH THE GENERATED DOCUMENTATION TO THE REPOSITORY. The detailed documentation should be placed in the header files, and in the source files only if the interface is not exposed in the header files.

## Packaging Guidelines

Follow the following workflow for packaging a new software:

1. Create a new directory in `database/` with the package name in lowercase and hyphenated if multiword (e.g., `intel-oneapi-mkl`).
2. Search the web to understand its toolchain structure, compulsory dependencies, optional dependencies, and any special build instructions.
3. Add a `latest.yaml` file for the latest version of the package, if it is an older version, specify the version range this recipe is valid for (e.g. `1.0.0-1.2.0.yaml`).
4. Create the recipe in the YAML file. The default configuration should be as performant as possible on x86_64 and ARM64 architectures. However, if the performance requires some optional dependencies, they should be disabled by default and the user should be able to enable them during installation.
5. Verify the recipe by running `kez dbcheck --only <package>` to check its validity.
6. Verify the recipe by running `kez uconf <package> --silence` to generate a default user configurable file.
7. Verify the recipe by running `kez install <package> --silence --dry-run` to inspect the bash commands that will be executed during installation. Use `--config` to override the default option states in the user configurable YAML file.
8. Prompt the user if they want to install the package. If yes, install the package with all its dependencies. If error occurs during installation, go back to step 4 and fix the recipe. If no or the installation goes through successfully, the packaging process is complete. The user can review the recipe and push the changes to the repository. DO NOT OVERRIDE KEZ_WORKDIR at any point during the packaging process. Use `--env test` to test the recipe in a temporary environment if needed.

### Database Update Protocol

When updating the database, follow these steps:
1. Search the web to find the latest version of the package and its release notes.
2. If the package has a new version, check if the existing recipe is still valid for the new version. If not, move the existing recipe to a new file with the version range it is valid for (e.g., `1.0.0-1.2.0.yaml`) and create a new `latest.yaml` for the new version. Then, perform the packaging workflow for the new version.
3. If the existing recipe is still valid for the new version, update the `latest.yaml` file with the new version source URL.
4. Remove redundant versions of the package from the database if they are no longer needed. We only keep the unique major and middle version numbers of the package. For example, if we have `1.0.0` in the database and we find `1.0.2`, we can remove `1.0.0` and keep `1.0.2`. If we find `1.1.0`, we can keep both `1.0.2` and `1.1.0`. If we find `2.0.0`, we should keep all three versions. The goal is to keep the database clean and only have the unique major and middle version numbers of the package. The exception to this rule is if the package changes its build recipe or interface drastically between minor versions (e.g. `1.0.0` and `1.0.1`), in which case we should keep both versions in the database.
