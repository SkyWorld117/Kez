# Architecture

This document describes Kez's internal architecture from a developer's perspective.

## Pipeline Overview

Kez processes packages through a five-stage pipeline:

```
1. Parse       →  2. Resolve    →  3. Generate    →  4. Parse      →  5. Execute
Database YAML      Dependencies     User Config        User Config      Shell Plan
                                       YAML               YAML
```

1. **Parse**: Read package metadata from the `database/` YAML files.
2. **Resolve**: Build a dependency DAG and determine build order (reversed topological sort).
3. **Generate**: Produce a user-configurable YAML file from the resolved dependencies.
4. **Parse**: Read the user YAML and produce a dependency-ordered `BashCommandPlan`.
5. **Execute**: Run the generated bash commands via `scripts/install.sh`.

When any plan entry requires Python, the executor inserts one internal
`.kez-python-environment` node. It depends on the plan's `python` package and
uses `scripts/python_env.sh` to turn every `Toolchain::Python` plan node into an
exact distribution requirement. Requirements from the current plan are merged
with Python package nodes saved by earlier incremental installs. The complete
set is installed in one fresh-venv transaction before those nodes and their
native dependents are released. uv first compiles the transitive requirements
into a lock, then synchronizes the fresh venv from that lock. This prevents
concurrent environment writers and keeps removed distributions from lingering.
Downloads are cached in `<environment>/.tmp/uv-cache`. Native package
`site-packages` directories are registered through managed `.pth` files after
each package finishes.

Steps 1–4 are handled by the C++ backend. Step 5 is handled by the bash frontend.

## Component Map

### `src/cmdline_parser/`

Parses command-line configuration overrides (`-c` flags). Two files:
- `cmdline_parser.cpp` — parses `KEY=VALUE` pairs into an internal structure
- `traverse.cpp` — walks a YAML node tree and applies the parsed overrides

### `src/database/`

The database subsystem parses and caches the package YAML files. It is the entry point
for all metadata about packages.

| File | Responsibility |
|---|---|
| `build_parser.cpp` | Parses `build` section (configurations, stages, environment, options, conditions) |
| `condition_parser.cpp` | Parses condition expressions using EBNF grammar |
| `config.cpp` | Configuration data structures and in-memory cache |
| `config_parser.cpp` | Orchestrates all sub-parsers for a single recipe |
| `config_selector.cpp` | Selects the best-matching configuration variant |
| `database.cpp` | Top-level database caching layer |
| `parser_utils.cpp` | Shared parsing helpers |
| `source_parser.cpp` | Parses `source` definitions (git, tarball, script, PyPI) |

### `src/dependency_resolver/`

Builds a dependency graph and determines the installation order.

| File | Responsibility |
|---|---|
| `advisor.cpp` | Lookup table for default implementations (e.g., `blas` → `intel-oneapi-mkl` on x86_64) |
| `optional_dependencies.cpp` | Handles optional dependency selection |
| `requirements.cpp` | Evaluates dependency requirements |
| `resolve_dependencies.cpp` | Dependency DAG construction and resolution |
| `toposort.cpp` | Reversed topological sort for build ordering |

### `src/uconf_generator/`

Transforms resolved dependencies into a user-editable YAML file.

| File | Responsibility |
|---|---|
| `config_transformer.cpp` | Transforms parsed database configs into user-configurable YAML |
| `configurations_filter.cpp` | Filters configurations by toolchain |
| `environment_filter.cpp` | Filters environment variables for the user config |
| `options_filter.cpp` | Filters build options for the user config |
| `stages_filter.cpp` | Filters build stages for the user config |
| `uconf_generator.cpp` | Orchestrates the full generation process |

### `src/uconf_parser/`

Parses user configuration YAML into an executable bash command plan.

| File | Responsibility |
|---|---|
| `source_commands.cpp` | Generates bash commands for source download and unpack |
| `template_resolver.cpp` | Multi-pass `${}` template variable resolver |
| `user_config_parser.cpp` | Main parser: user YAML → `BashCommandPlan` |

### `src/factory/`

Batch build and profiling for multiple configurations.

| File | Responsibility |
|---|---|
| `factory.cpp` | Parses factory config, wraps targets with launchers/schedulers |

### `src/rebuild/`

Computes the transitive closure of dependents for targeted rebuilds.

| File | Responsibility |
|---|---|
| `rebuild.cpp` | Loads installed packages, builds dependents map, computes rebuild sets, filters plans |

### `src/ui/`

Command-line interface, implemented with lightweight manual argument parsing.

| File | Responsibility |
|---|---|
| `ui.cpp` | Command dispatcher |
| `environment.cpp` | `env`, `compiler`, `mpi`, `vendor` commands |
| `factory.cpp` | `factory` commands |
| `init.cpp` | `init` and `update` commands |
| `install.cpp` | `install` and `utilities` commands |
| `packages.cpp` | `info`, `uconf`, `dbcheck` commands |
| `ui_utils.cpp` | Shared utilities: path resolution, environment activation, etc. |
| `bash_completion.cpp` | Bash completion logic (used by `kez_completion` binary) |

### `src/utils/`

Shared utility functions.

| File | Responsibility |
|---|---|
| `bash_utils.cpp` | Shell quoting, env var helpers |
| `file_utils.cpp` | Filesystem operations |
| `string_utils.cpp` | String splitting, trimming, joining |
| `terminal_ui.cpp` | Raw-terminal single/multiple selection controls |
| `yaml_utils.cpp` | YAML serialization helpers |
| `colored_io/` | Colored terminal output (info/success/warning/error) |

### `bash_completion_main.cpp`

Entry point for the `kez_completion` binary that provides bash completion.

## Code Layout

```
include/          # C++ headers, mirroring src/ structure
src/              # C++ source files
tests/            # Googletest-based unit tests
scripts/          # Bash scripts (init.sh, install.sh, python_env.sh, gen_modulefile.sh)
main.sh           # Bash wrapper — intercepts env/compiler/mpi/factory commands
setup-env.sh      # Environment setup — sources main.sh, configures PATH
completion.bash   # Bash completion registration
manifest.yaml     # Project metadata, dependency versions, system stack, paths
```

## `KEZ_WORKDIR` Layout

```
$KEZ_WORKDIR
├── config.yaml
├── env
    ├── system           # Core toolchain (built by kez init)
    ├── utilities        # Shared utility packages
    ├── compilers        # Compiler installations (gcc-x.x.x, llvm-x.x.x)
    ├── mpis             # MPI installations (openmpi-x.x.x-gcc-x.x.x)
    ├── vendors          # Vendor SDKs (nvhpc-x.x, oneapi-x.x.x.x)
    └── applications     # User application environments
├── factories            # Factory directories
├── modulefiles          # Tcl modulefiles for environment-modules integration
└── .cache               # Cache directory
```

Paths are configured in `manifest.yaml` under the `paths` key and resolved relative to
`$KEZ_WORKDIR`. The `config.yaml` at the root controls build settings and external
package locations.

An application environment that uses Python additionally contains `.venv/`
and `.kez-python/`. The former is the runtime virtual environment; the latter
stores the base-interpreter signature, per-package declarations, the combined
requirements, and the transitive lock generated by `uv pip compile`.

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

This manifest drives `setup-env.sh` path resolution and `scripts/init.sh` package ordering.
