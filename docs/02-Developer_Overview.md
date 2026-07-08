# Developer Overview

This document provides an overview of the developer's perspective on the Kez project, including its structure, components, and development practices.


## Source Code Structure

The source code of Kez is organized into several key directories:

- `database/`: Contains all the package metadata in YAML format (one directory per package with a `latest.yaml` file)
- `docs/`: Contains all the documentation files
- `include/`: Contains the C++ header files
- `src/`: Contains the C++ source code of Kez, including the core logic and utilities
- `tests/`: Contains the C++ test files
- `scripts/`: Contains bash scripts for initialization and installation
- `main.sh`: The main script that acts as a bridge between the bash frontend and the C++ backend
- `setup-env.sh`: A script to set up the Kez environment
- `manifest.yaml`: Project manifest with dependency versions, system-stack definitions, and path configuration

### `src` Directory

```
src/
 ├── cmdline_parser/
 ├── utils/
 ├── database/
 ├── dependency_resolver/
 ├── factory/
 ├── parser/
 ├── user_config_generator/
 ├── ui/
```

#### `cmdline_parser`

`cmdline_parser` offers commandline configuration parsing, making the build system simple and efficient to use when no complicated options are required. It has two components:

- `cmdline_parser.cpp`: Parses command-line overrides for package configuration values
- `traverse.cpp`: Recursively traverses a YAML node tree and applies parsed overrides

#### `utils`

`utils` contains utility functions that are used across different modules. These functions can include file handling, string manipulation, logging, and other common tasks that are needed in various parts of the codebase.

- `bash_utils.cpp`: Shell command execution, quoting, environment variable helpers
- `file_utils.cpp`: Filesystem utilities
- `string_utils.cpp`: String splitting, trimming, joining
- `yaml_utils.cpp`: YAML serialization and file writing
- `colored_io/`: Colored terminal output (info, success, warning, error)

#### `database`

`database` is a caching and parsing system for the internal package database. It is supposed to drastically reduce the I/O and parsing overhead when handling dependency solving and parsing. It consists of:

- `build_parser.cpp`: Parses build configurations (configurations, stages, environment, options, conditions)
- `condition_parser.cpp`: Parses conditional logic using EBNF grammar
- `config.cpp`: Configuration data structures and caching
- `config_parser.cpp`: Top-level configuration parser orchestrating all sub-parsers
- `config_selector.cpp`: Selects the best-matching configuration from available options
- `database.cpp`: Database caching system
- `parser_utils.cpp`: Shared utility functions for YAML parsing
- `source_parser.cpp`: Parses source definitions (git, tarball, script)

#### `dependency_resolver`

`dependency_resolver` is responsible for resolving package dependencies. It builds a dependency graph as a directed acyclic graph (DAG) and determines the build order according to the reversed topological sort of the graph.

Notice that it also handles the part of user selection of implementations for abstract packages as well as if certain optional dependencies should be included.

This module is mainly used by `user_config_gen` to generate the user configuration file based on the resolved dependencies. It consists of:

- `advisor.cpp`: Lookup table for default implementation suggestions (e.g., `blas` → `intel-oneapi-mkl` on x86_64)
- `essential_dependencies.cpp`: Collects non-optional dependencies from the parsed database
- `optional_dependencies.cpp`: Handles optional dependency selection
- `requirements.cpp`: Evaluates dependency requirements
- `resolve_dependencies.cpp`: Main dependency resolution algorithm (DAG construction)
- `toposort.cpp`: Topological sort for build ordering

#### `factory`

`factory` provides batch instantiation and profiling capabilities for configurations. It manages factory directories containing multiple recipe files and allows building all configurations in one command. See the [Factories documentation](07-Factories.md) for more details.

#### `parser`

`parser` parses user configuration files against the typed package metadata and produces a dependency-ordered `BashCommandPlan`. Each plan entry contains a package name and its bash commands. The plan remains a C++ data structure so the installation executor does not need an intermediate instruction YAML file.

- `source_commands.cpp`: Generates bash commands for downloading and unpacking source code
- `template_resolver.cpp`: Resolves `${}` template variables in configuration values
- `user_config_parser.cpp`: Parses user configuration YAML into a `BashCommandPlan`

#### `user_config_generator`

`user_config_generator` is responsible for generating the user configuration file based on the resolved dependencies. It takes the output from the `dependency_resolver` and produces a YAML file that can be used by the package manager.

- `config_transformer.cpp`: Transforms parsed database configurations into user-configurable YAML
- `configurations_filter.cpp`: Filters build configurations based on toolchain
- `environment_filter.cpp`: Filters environment variables
- `options_filter.cpp`: Filters build options
- `stages_filter.cpp`: Filters build stages
- `user_config_generator.cpp`: Orchestrates the generation process

#### `ui`

`ui` handles the command-line interface for Kez. Unlike the old argparse-based system, Kez uses a lightweight manual argument parsing approach.

- `bash_completion.cpp`: Bash completion logic (used by the `kez_completion` binary)
- `environment.cpp`: Environment management (`env create/remove/list/enter/exit/which/empty`)
- `factory.cpp`: Factory subcommands (`factory create/remove/list/enter/exit/which/build/run`)
- `init.cpp`: Initialization subcommand
- `install.cpp`: Installation subcommand (generates shell plan)
- `packages.cpp`: Package listing and info subcommands
- `ui.cpp`: Main UI dispatcher
- `ui_utils.cpp`: Shared UI utility functions


## `$KEZ_WORKDIR` Structure

```
$KEZ_WORKDIR
├── config.yaml
├── env
    ├── system
    ├── utilities
    ├── compilers
        ├── gcc-x.x.x
        ├── llvm-x.x.x
        ├── ...
    ├── mpis
        ├── openmpi-x.x.x-gcc-x.x.x
        ├── openmpi-x.x.x-llvm-x.x.x
        ├── ...
    ├── vendors
        ├── nvhpc-x.x
        ├── oneapi-x.x.x.x
        ├── ...
    ├── applications
        ├── my-app
        ├── ...
```

As discussed in [Getting Started](01-Getting_Started.md#configuring), `config.yaml` contains the basic configurations of Kez.

Here we focus on `env`. `env` contains a collection of prefixes that act like virtual environments for different end-user applications. For example, one can create a `CONQUEST-1.4` application environment to install specific versions of dependencies required by that application without affecting other applications.

There are special environments:

- `system`: Contains system-like toolchains. All the packages installed by `kez init` are stored in this environment.
- `utilities`: Contains utilities that are not critical to the whole system, e.g. monitor tools like `htop`.
- `compilers`: Contains prefixes of compilers. Each compiler with a specific version is installed in a separate directory, e.g. `gcc-x.x.x`, `llvm-x.x.x`.
- `mpis`: Contain prefixes of MPI implementations. Each MPI implementation with a specific version is installed in a separate directory, e.g. `openmpi-x.x.x-gcc-x.x.x`, `openmpi-x.x.x-llvm-x.x.x`.
- `vendors`: Contain prefixes of vendor-specific toolchains. Each vendor with a specific version is installed in a separate directory, e.g. `nvhpc-x.x`, `oneapi-x.x.x.x`.

This design choice allows sharing less mutable packages across different applications, reducing redundancy and build time, and saving disk space, while still offering a high degree of isolation between applications.


## `manifest.yaml`

The `manifest.yaml` file at the project root centralizes key metadata:

```yaml
project:
    name: Kez
    abbrev: kez
    version: dev

dependencies:
    patchelf: latest
    googletest: latest
    yaml-cpp: latest
    yq: latest

system-stack:
    gmp: latest
    mpfr: latest
    mpc: latest
    isl: latest
    zlib: latest
    xz: latest
    lz4: latest
    zstd: latest
    binutils: latest
    gcc: 11.5.0
    elfutils: latest
    m4: latest
    autoconf: latest
    automake: latest
    libtool: latest
    make: latest
    rust: latest
    cmake: latest
    perl: latest
    git: latest

paths:
    environment: env
    system: env/system
    utilities: env/utilities
    compilers: env/compilers
    mpis: env/mpis
    vendors: env/vendors
    applications: env/applications
    cache: .cache
    factories: factories
    modulefiles: modulefiles
```

This manifest drives `setup-env.sh` path resolution and `scripts/init.sh` package ordering, replacing the old hardcoded path `fromager.paths` config section.
