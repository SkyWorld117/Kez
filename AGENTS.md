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

## Repository Structure

The repository mixes a C++ backend, a bash frontend, and a YAML data layer. Top-level layout:

```
Kez/
├── include/        # C++ headers, mirroring the src/ component layout
├── src/            # C++ sources, mirroring the include/ component layout
├── tests/          # Google Test cases, one *_test.cpp per component
├── database/       # Package recipes: <name>/latest.yaml (one dir per package)
├── archive/        # Older/example package recipes kept for reference, not active
├── patches/        # Source patches applied during builds, grouped by package
├── configs/        # Example user configuration files (config.yaml + cluster presets)
├── heuristics/     # Architecture- and arch-specific advice tables (YAML)
├── scripts/        # Bash scripts driven by the C++ CLI (init, install, modulefile)
├── tools/          # Standalone shell helpers used by recipes (unpack, rpath, etc.)
├── docs/           # Numbered developer/user documentation (Markdown)
├── bin/            # Build output: compiled binaries (gitignored)
├── lib/            # Build output: compiled static library (gitignored)
├── obj/            # Build output: object files (gitignored)
├── .cache/         # clangd cache (gitignored)
├── Makefile        # Builds the library, CLI, completion, and print_* binaries
├── main.sh         # Bash wrapper that forwards to bin/kez (evals env/compiler/mpi/factory)
├── setup-env.sh    # Sets KEZ_HOME/KEZ_WORKDIR and sources the shell entry points
├── completion.bash # Bash completion hook calling bin/kez_completion
├── manifest.yaml   # Project metadata + system-stack dependency list
├── generate_compile_commands.py  # Emits compile_commands.json for clangd via make -n
├── AGENTS.md       # This file
└── README.md
```

### C++ backend (`include/` + `src/`)

Headers and sources share the same component subdirectories. Each component corresponds to a stage in the pipeline described in the Project Overview:

- `cmdline_parser/` — Parses the user's command line (`kez install ...`) into a structured plan. `traverse.hpp` walks the parsed command tree.
- `database/` — Parses package recipe YAML from `database/`. Split into focused parsers: `source_parser`, `build_parser`, `config_parser`, `condition_parser`, plus `config_selector` and the top-level `database` aggregator. `parser_context`/`parser_utils` are shared parsing helpers.
- `dependency_resolver/` — Resolves the dependency graph. `resolve_dependencies` + `toposort` produce an install order; `optional_dependencies` classify edges; `requirements` captures constraints; `advisor` consults `heuristics/` to pick arch-appropriate substitutes (e.g. BLAS → MKL/NVPL).
- `uconf_generator/` — Transforms the resolved dependency set into the user-editable YAML config. A chain of filters: `configurations_filter`, `options_filter`, `environment_filter`, `stages_filter`, unified by `config_transformer` and `uconf_generator`.
- `uconf_parser/` — Parses the *generated* user config back into executable commands. `user_config_parser` is the entry point; `source_commands` and `template_resolver` (with `parser_internal`) handle template/`uconf` expansion. (Implementation lives in `src/uconf_parser/`.)
- `rebuild/` — Powers `install --rebuild <pkg>` (a.k.a. `-R`): reads an environment's installed set from `state.yaml`, inverts a `BashCommandPlan`'s dependency edges into a dependents map, and computes the transitive closure of a target's dependents to produce the filtered rebuild plan. Pure functions over `BashCommandPlan` (from `uconf_parser/`).
- `ui/` — The interactive front end: `commands`/`ui` dispatch subcommands (`init`, `install`, `packages`, `environment`, `factory`, ...), `ui_utils` and `bash_completion` support them. `install.cpp` hosts the `install --rebuild`/`-R` path, which narrows the plan to the rebuild set and forces it through `scripts/install.sh`.
- `factory/` — Parses factory profiles (buildspace/launch/scheduler templates) emitted as part of the install plan.
- `utils/` — Generic helpers shared across components: `yaml_utils`, `string_utils`, `file_utils`, `bash_utils`, `dump`, `terminal_ui`, and `colored_io` (the `ERROR`/info/warning/success printers; `colors.h` holds the raw ANSI codes).

Entry points: `src/main.cpp` builds the `kez` CLI binary (`run_ui`); `src/bash_completion_main.cpp` builds the `kez_completion` helper. The Makefile also produces a small `kez_print` binary used by shell scripts.

### Bash frontend (`scripts/` + `tools/` + root shims)

The C++ CLI emits a shell plan; these scripts execute it:

- `scripts/init.sh` (+ `init_utils.sh`) — Bootstraps a nearly distro-independent system stack under `KEZ_WORKDIR`.
- `scripts/install.sh` — Runs the plan emitted by the command-line parser; owns process execution and installed-state tracking only (no metadata reparsing).
- `scripts/gen_modulefile.sh` — Generates a Tcl environment modulefile from an installed environment.
- `tools/` — Recipe-level helpers (`unpack.sh`, `shallow_clone.sh`, `patch_rpath.sh`, `patch_gcc_linking.sh`, `get_missing_libs.sh`) invoked from build steps.
- `main.sh` — The `kez` shell function; forwards to the binary but intercepts commands that must affect the *current* shell (`env activate/deactivate`, `compiler load/unload`, `mpi load/unload`, `factory enter/exit`) and evaluates their output.
- `setup-env.sh` — Establishes `KEZ_HOME`/`KEZ_WORKDIR`, copies a default `config.yaml`, and sources `main.sh`/`completion.bash`.

### Data layer (`database/` + `archive/` + `patches/` + `configs/` + `heuristics/`)

- `database/<pkg>/latest.yaml` — One recipe per package. A recipe declares its source tarballs/versions, dependencies, and build configuration (options, configurations, postprocessing). See `docs/03-Database_Configuration_Format.md`.
- `archive/` — Superseded or example recipes, not part of the active database.
- `patches/<pkg>/` — `.patch` files applied to a package's unpacked source.
- `configs/` — Example user configs: `config.yaml` (the default copied by `setup-env.sh`) plus cluster presets (`piora.yaml`, `sbrinz.yaml`).
- `heuristics/` — `architecture.yaml` maps tool→architecture triples (e.g. llvm/cuda/nodejs targets per x86_64/arm64); `advice.yaml` maps abstract packages to concrete arch-appropriate ones (BLAS/LAPACK/FFTW/MPI → MKL/NVPL/armpl/openmpi).

### Documentation (`docs/`)

Numbered Markdown guides covering the end-to-end flow: Getting Started, Developer Overview, Database Configuration Format, Abstract Configuration Format, Templating, User Configuration Format, and Factories.

## Compiling and Testing

Compiling the project is done using `make`. The `Makefile` is located in the root directory of the project. To compile the project, simply run:

```bash
make -j
```

Testing is done similarly via `make -j test`. Notice that running the tests does not necessarily fully compile the project.

## Documentation

The documentation is located in the `docs/` directory. It contains structured guides and references for both users and developers. 

Next to the documentation, it is also required to write doxygen-style comments in the codebase. This will allow for automatic generation of API documentation. Run `doxygen` at the root of the project to generate the documentation. DO NOT PUSH THE GENERATED DOCUMENTATION TO THE REPOSITORY. It is only meant for local use. The detailed documentation should be placed in the header files, and in the source files only if the interface is not exposed in the header files.
