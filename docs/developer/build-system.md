# Build System

Kez uses GNU Make as its build system.

## Building Kez

### Prerequisites

Set up the build environment before compiling:

```bash
source setup-env.sh
```

This sets `KEZ_WORKDIR`, `KEZ_HOME`, and `KEZ_NPROC`, and adds the system toolchain
to `PATH`.

### Make Targets

| Target | Description |
|---|---|
| `all` (default) | Build `libkez.a`, `kez` binary, `kez_completion` binary, and print helpers |
| `kez` | Build only the `kez` CLI binary |
| `test` | Build and run the test suite |
| `clean` | Remove build artifacts (`obj/`, `bin/`, `lib/`) |

### Build Artifacts

| Artifact | Path | Description |
|---|---|---|
| Static library | `lib/libkez.a` | Core library (parser, resolver, database, utils) |
| CLI binary | `bin/kez` | Main `kez` command |
| Completion binary | `bin/kez_completion` | Bash completion helper |
| Test binary | `bin/test_database` | Unit test runner |
| Print helper | `bin/kez_print` | Colored output tool (takes type: info/warning/error/success) |

### Compile Options

The Makefile respects the standard variables:

```bash
make CXX=g++ CXXFLAGS="-O3 -flto"   # Production build (default)
make CXXFLAGS="-g -O0 -DDEBUG"      # Debug build
make -j$(nproc)                      # Parallel build
```

## Test Suite

Kez uses [Google Test](https://github.com/google/googletest) for C++ unit tests.

```bash
# Build and run all tests
make test

# Run tests manually (after building)
./bin/test_database
```

### Test Files

| Test | Lines | What it covers |
|---|---|---|
| `user_config_parser_test.cpp` | 602 | User config YAML parsing and BashCommandPlan generation |
| `user_config_generator_test.cpp` | 451 | User config YAML generation from resolved dependencies |
| `dependency_resolver_test.cpp` | 306 | Dependency DAG construction and topological sort |
| `database_test.cpp` | 234 | Database caching and config parsing |
| `cmdline_parser_test.cpp` | 182 | Command-line override parsing |
| `bash_completion_test.cpp` | 126 | Bash completion logic |
| `rebuild_test.cpp` | ~80 | Rebuild set computation and plan filtering |
| `factory_test.cpp` | 63 | Factory config parsing |
| `utils_test.cpp` | 19 | Utility functions |

## Code Formatting

Kez uses `clang-format` (Google style with customizations) and `pre-commit` hooks
to enforce consistent formatting.

```bash
# Install pre-commit
pip install pre-commit

# Install the git hooks
pre-commit install

# (Optional) Run on all files
pre-commit run --all-files
```

The configuration is in `.clang-format` and `.pre-commit-config.yaml`.

Formatting is checked automatically before each commit. If your code doesn't match,
the hook will reformat it. Stage the changes and commit again.

## LSP Support

A helper script generates `compile_commands.json` for clangd and other LSP clients:

```bash
python generate_compile_commands.py
```

This parses `make --dry-run` output to produce the compilation database.

## Coding Style

- Use a **function-heavy C-style** approach in C++ code. Avoid complex classes unless
  there is a clear benefit.
- Standard C++ containers (`std::vector`, `std::map`, etc.) are acceptable.
- **No exceptions.** The program terminates immediately with `exit(EXIT_FAILURE)` on
  any error. Use `ERROR()` from `colored_io.hpp` for error reporting.
- Generic utilities go in `include/utils/` and `src/utils/`. Component-specific
  utilities stay in the component's directory.
- Tests go in `tests/` following the naming convention `<component>_test.cpp`.
