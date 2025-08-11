# Developer Overview

This document provides an overview of the developer's perspective on the Fromager project, including its structure, components, and development practices.


## Source Code Structure

The source code of Fromager is organized into several key directories:

- `bin/`: Contains executable scripts and Fromager's binaries that are built during `fgr init`
- `database/`: Contains all the package metadata in YAML format
- `docs/`: Contains all the documentation files
- `src/`: Contains the source code of Fromager, including the core logic and utilities (see more in [`src` Directory](#src-directory))
- `main.sh`: The main script that acts as a bridge between the bash frontend and the C++ backend
- `setup-env.sh`: A script to set up the Fromager environment

### `src` Directory

```
src/
 ├── colors/
 ├── dependency_resolver/
 ├── package_format_verifier/
 ├── parser/
 ├── user_config_gen.cpp
 ├── ...
```

#### `colors`

`colors` contains the utilities for colored output in the terminal. It is not only used in other C++ programs in the format of header files, but also compiled into binaries and used by the bash scripts for consistent output formatting. They are designed to only output colored text in an interactive shell, and regular text if not (e.g. piped into a file).

#### `dependency_resolver`

`dependency_resolver` is responsible for resolving package dependencies. It builds a dependency graph as a directed acyclic graph (DAG) and determines the build order according to the reversed topological sort of the graph.

Notice that it also handles the part of user selection of implementations for abstract packages as well as if certain optional dependencies should be included.

This module is mainly used by `user_config_gen` to generate the user configuration file based on the resolved dependencies.

#### `package_format_verifier`

`package_format_verifier` is a helper tool of package developers to verify whether their YAML configuration files are in the correct format. It can also be used as a self check tool for Fromager.

Although `parser` also performs certain degrees of checking, `package_format_verifier` is supposed to offer more comprehensive reports when encountering issues. `package_format_verifier` is developer-focused while `parser` is user-focused.

#### `parser`

`parser` is responsible for parsing the user configuration files and associating them with the appropriate package metadata. It ensures that the user configurations are correctly formatted and contain all necessary information for the package manager to function properly.

It outputs structured bash commands based on the configurations, stored in `${TARGET_CELLAR}/.tmp/ins.yaml`.

#### `user_config_gen`

`user_config_gen` is responsible for generating the user configuration file based on the resolved dependencies. It takes the output from the `dependency_resolver` and produces a YAML file that can be used by the package manager.


## `$FROMAGER_WORKDIR` Structure

```
$FROMAGER_WORKDIR
├── config.yaml
├── cellars
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
    ├── ... [<Your custom cellars>]
```

As discussed in [Getting Started](01-Getting_Started.md#configuring) `config.yaml` contains the basic configurations of Fromager.

Here we focus on `cellars`. `cellars` contains a collection of prefixes that act like virtual environments for different end-user applications. For example, one can create a `CONQUEST-1.4` cellar to install specific versions of dependencies required by that application without affecting other applications.

There are special cellars:

- `system`: Contain system-like toolchains. All the packages installed by `fgr init` are stored in this cellar.
- `utilities`: Contain utilities that are not critical to the whole system, e.g. monitor tools like `htop`.
- `compilers`: Contain prefixes of compilers. Each compiler with a specific version is installed in a separate directory, e.g. `gcc-x.x.x`, `llvm-x.x.x`.
- `mpis`: Contain prefixes of MPI implementations. Each MPI implementation with a specific version is installed in a separate directory, e.g. `openmpi-x.x.x-gcc-x.x.x`, `openmpi-x.x.x-llvm-x.x.x`.
- `vendors`: Contain prefixes of vendor-specific toolchains. Each vendor with a specific version is installed in a separate directory, e.g. `nvhpc-x.x`, `oneapi-x.x.x.x`.

This design choice allows sharing less mutable packages across different applications, reducing redundancy and build time, and saving disk space, while still offering a high degree of isolation between applications.