# Recipe Schema

This document describes the YAML schema for package recipes in the database. Each package has a directory under `database/<package_name>/` containing a `latest.yaml` file.

```
database/
 ├── gcc/
 │   └── latest.yaml
 ├── openmpi/
 │   └── latest.yaml
 ├── zlib/
 │   └── latest.yaml
 ├── scotch/
 │   ├── latest.yaml
 │   └── 6.1.3-6.1.3.yaml   # Version-specific recipe
 ...
```

Most packages contain a single `latest.yaml` file. A package may also hold
version-specific files named `<version>.yaml` alongside `latest.yaml`. Both
formats are parsed identically.


## Concrete Package Format

All concrete (non-abstract) packages share this structure. Optional fields are marked
with _(optional)_.

```yaml
recipe:
  name: <package_name>
  description: <package_description>       # _(optional)_
  author: <author_name>                    # _(optional)_
  type: <package_type>
  toolchain: <toolchain_name>              # _(optional)_
  source: <source_definition>              # _(optional)_
  dependencies: <dependencies>             # _(optional)_
  overrides: <overrides>                   # _(optional)_
  build:                                   # _(optional)_
    preprocessing: <preprocessing_commands>
    postprocessing: <postprocessing_commands>
    configurations: <configuration_definition>
    stages: <build_stages>
  properties: <properties_definition>      # _(optional)_
```

### `type`

Determines where the package is installed:

| Type | Installation path | Notes |
|---|---|---|
| `package` | `env/applications/<env_name>` | Regular applications (90 packages) |
| `system` | `env/system` | Core toolchain libraries (8 packages) |
| `compiler` | `env/compilers/<name>-<version>` | Compiler installations (2 packages) |
| `mpi` | `env/mpis/<name>-<version>-<compiler>` | MPI implementations (2 packages) |
| `vendor` | `env/vendors/<name>-<version>` | Vendor SDKs (8 packages) |
| `external` | (configured in `config.yaml`) | System-provided packages (4 packages) |
| `abstract` | Not installed directly | Interface to concrete packages (see below) |

### `source`

Describes how Kez fetches the source code or prebuilt binaries.

| Source type | Description |
|---|---|
| `tarball` | `.tar.gz`, `.tar.xz`, `.tgz` archives |
| `git` | Git repository (uses `tag` field) |
| `script` | Self-contained installer script |
| `zip` | `.zip` archives |

```yaml
source:
  type: git | tarball | zip | script
  url: <repository_or_download_url>          # Used for git repos, or as fallback URL
  releases:
    - version: <release_version>
      url: <release_specific_url>            # Overrides top-level url for tarball/zip/script
      tag: <release_tag>                     # Used if type is `git`
```

For `script` type, `url` is optional — the script logic goes in `preprocessing`/`postprocessing`.

### `dependencies`

A simple YAML list of package names. Version constraints are not yet supported.

```yaml
dependencies:
  - zlib
  - hdf5
  - boost
```

### `toolchain`

| Toolchain | Behaviour |
|---|---|
| `autotools` | Generates `./configure` commands with `--`-prefixed options |
| `cmake` | Generates `cmake ..` commands with `-D`-prefixed options |
| `make` | Plain `make`; options are injected as environment variables |
| _(none)_ | No standard build-system wrappers; use `preprocessing`/`postprocessing` |

Abstract and external packages do not have a toolchain. Vendor packages using a
`script` source type also typically have no toolchain — their installation logic
goes in `preprocessing`/`postprocessing`.

For packages with no toolchain, configuration instructions are only generated
when `build.configurations.command` is explicitly set. If the `command` field
is absent, the configuration options and environment variables are still
available for **templating** (e.g. `${pkg.config.*}`, `${pkg.env.*}`), but no
shell commands are emitted for that configuration block. This is useful for
packages like `intel-oneapi-mkl` that declare configurable options exclusively
for dependent packages to reference via the template system.

> **Stages follow the same rule.** For packages with no toolchain, stages
> produce commands only when they carry their own `configurations.command`
> (see [`stages`](#stages) below). Stages without a `configurations` block
> — or with a `configurations` block that lacks an explicit `command` — are
> silently dropped because there is no toolchain default to fall back on.

### `overrides`

Modifies build parameters of dependencies. Defined in the schema but not currently
used by any package in the database. The `target` can be any template variable.

```yaml
overrides:
  - condition: <condition>
    target: <template_variable>
    action: append | prepend | set    # default: set
    value: <value>
```

### `build`

#### `configurations`

```yaml
configurations:
  command: <configuration_command>          # Overrides the default build command (rare)
  environment:
    - <environment_variable_definition>
  options:
    - <option_definition>
```

The `command` field overrides the build command for the toolchain entirely.
Used by a few packages with custom build systems (e.g., `boost` uses `./b2`,
`openfoam` sources a setup script before building).

If `configurations` is empty (`{}`), the toolchain default is used with no
custom options or environment variables.

For packages with **no** `toolchain` field, the `command` is **required** to
generate any configuration instructions. When absent, the configuration's
options and environment variables are still computed for template resolution
(e.g. `${pkg.config.*}`, `${pkg.env.*}`), but no shell commands are emitted.
This allows packages to expose configurable properties to dependents without
producing a configure step.

#### `stages`

```yaml
stages:
  - target: <make_target>
    multithreaded: true | false        # default: true
    configurations: <configurations_list>
  - ...
```

A `target` of `~` (null) means the default target.

##### Stage commands depend on the toolchain

For **toolchain-backed packages** (`autotools`, `cmake`, `make`), each stage
produces a command using the toolchain's default, optionally overridden by
the stage's `configurations.command`:

| Toolchain | Default for stage (null target) | Default for install target |
|---|---|---|
| `autotools` | `make -j{N}` | `make install` |
| `cmake` | `cmake --build build --parallel {N}` | `cmake --install build` |
| `make` | `make -j{N}` | `make install` |

For packages **with no toolchain** (generic), `default_stage_command()` returns
nothing — stages produce commands **only** when they carry an explicit
`configurations.command`:

```yaml
# Generic package: stage produces no command without explicit command
stages:
  - target:
  - target: install

# Generic package: stage works with explicit command
stages:
  - target:
    configurations:
      command: make -j${kez.arch.cores}
  - target: install
    configurations:
      command: make install
```

Stages that produce no commands are **silently dropped** during user-config
generation — they do not appear in the generated YAML and are never executed.
This applies to all packages, not just generic ones.

> **Rule of thumb:** If a stage has no `configurations` block at all, it is
> discarded. If it has a `configurations` block but no `command`, the
> configuration's options and environment variables are still computed for
> template resolution (e.g. `${pkg.config.*}`, `${pkg.env.*}`), but no shell
> command is emitted for the stage unless a toolchain default exists or the
> `command` is explicitly set.

Some packages have no `stages` at all, only `preprocessing`/`postprocessing`.
This is common for binary installers that copy prebuilt files rather than
compiling from source (e.g., `cmake`, `cuda`, `nodejs`), and for generic
packages that handle all the work through the top-level `configurations`
command or `preprocessing`/`postprocessing`.

#### `environment`

```yaml
environment:
  - name: <variable_name>
    description: <variable_description>
    user_configurable: true | false    # default: false
    default: <value>
    conditions: <conditions_block>
```

#### `options`

```yaml
options:
  - name: <option_name>
    description: <option_description>
    user_configurable: true | false    # default: false
    enabled:
      default: true | false
      conditions: <conditions_block>
    enabled_format: <format_string>    # e.g., "enable-feature" — default: <option_name>
    disabled_format: <format_string>   # e.g., "disable-feature" — default: empty (skip flag)
    requires: [<dependency_name>]
    enabled_value:
      default: <value>
      conditions: <conditions_block>
    disabled_value:
      default: <value>
      conditions: <conditions_block>
```

**Naming rules:**

| Toolchain | Prefix added | Example |
|---|---|---|
| Autotools | `--` | `enable-mpi` → `--enable-mpi` |
| CMake | `-D` | `BUILD_SHARED_LIBS` → `-DBUILD_SHARED_LIBS` |
| Make / _(none)_ | (none) | `CFLAGS` stays `CFLAGS=...` |

Existing leading prefixes in option names are accepted for custom commands and
backward compatibility. The user-config generator supplies standard toolchain options
(install prefix, compilers, language flags, linker flags) when the database does not
declare them; a database option with the same logical name takes precedence.

#### `conditions`

```yaml
conditions:
  - condition: <condition_expression>
    action: append | prepend | set    # default: set
    value: <value>
```

Conditions are evaluated top-to-bottom. The value of the first `set` action whose
condition matches is used. Actions `append` and `prepend` modify the value from previous
matching conditions.

**Condition syntax (EBNF):**

```ebnf
condition =
    "environment" <variable> |
    <option> <enabled> [<enabled_value>] |
    <option> <disabled> [<disabled_value>] |
    "version" <self.version><op><version>[,<op><version>] |
    <condition> && <condition> |
    <condition> || <condition> |
    "not" <condition> |
    "(" <condition> ")" |
    true | false
```

### `properties`

```yaml
properties:
  parent: <parent_package>              # For submodules of vendor packages
  prefix: <custom_prefix>               # Overrides the default package prefix
  c: <c_compiler>                       # For compiler or mpi types
  cxx: <cxx_compiler>                   # For compiler or mpi types
  fort: <fortran_compiler>              # For compiler or mpi types
  omp_flags: <omp_flags>                # For compiler or mpi types
  include: ${package.prefix}/include
  lib: ${package.prefix}/lib
  libs:
    default: <default_libs>
    conditions:
      - condition: <condition>
        value: <libs_value>
```

`include` and `lib` contain paths, not compiler flags. The parser derives
`${package.includes}`, `${package.ldflags}`, and `${package.nvldflags}` from them.
Explicit `includes`, `ldflags`, or `nvldflags` properties override the derived form.

For Autotools and CMake configurations, raw include/library paths from selected
dependencies are written to generated compile/link options automatically. Linker/RPATH
syntax is selected from the package's configured compiler (`-Wl,...` for GNU-compatible
drivers, `-Xlinker ...` for NVIDIA).


## Abstract Package Format

Abstract packages serve as interfaces that redirect to concrete implementations.
They live in the same database directory structure.

```yaml
recipe:
  name: <abstract_package_name>
  description: <abstract_package_description>
  type: abstract
  implementations:
    - <concrete_package_1>
    - <concrete_package_2>
```

Abstract packages typically have no `source`, `build`, `dependencies`, or `properties`
sections. The concrete packages define their properties under the abstract package name
to provide a unified interface.

### Property Namespace Sharing

Concrete implementations of an abstract package declare their properties under
simple names (e.g. `c`, `cxx`, `fort`), and the template system resolves
abstract-name references (e.g. `${mpi.c}`) by first mapping the abstract name
to its concrete implementation, then looking up the simple property on that
config. This convention provides a stable, unified property namespace for
dependents regardless of which implementation is selected.

For example, the abstract `mpi` package is implemented by `openmpi` and `mpich`.
Template resolution works as follows:

| Template reference | When `mpi→openmpi` | When `mpi→mpich` |
|---|---|---|
| `${mpi.c}` | `${openmpi.prefix}/bin/mpicc` | `${mpich.prefix}/bin/mpicc` |
| `${mpi.cxx}` | `${openmpi.prefix}/bin/mpicxx` | `${mpich.prefix}/bin/mpicxx` |

A recipe that depends on `mpi` can reference `${mpi.cxx}` and the template
resolver will redirect to the selected concrete package's property —
no recipe needs to know whether `openmpi` or `mpich` was chosen.

This convention is not enforced by the parser but is critical for correct
template resolution across different implementations.

### The `.use-<concrete>` Mechanism

When a user configuration is parsed and an abstract package is resolved to a
concrete implementation, the system auto-generates boolean named option states
with the pattern `<abstract_name>.use-<concrete_name>` for every implementation
of each abstract package. These states are injected into the option state and
can be used in condition expressions.

For example, when `mpi` is resolved to `openmpi` via
`recipe.abstract_packages: { mpi: openmpi }`, the parser creates:

| Property | Value |
|---|---|
| `mpi.use-openmpi` | `true` |
| `mpi.use-mpich` | `false` |
| `mpi.use-intel-oneapi-mpi` | `false` |
| _(...and so on for each implementation)_ |

These properties enable conditional configuration based on the chosen
implementation:

```yaml
conditions:
  - condition: ${mpi.use-mpich} true
    action: set
    value: -DMPICH_FOUND
```

### Selection

When generating a user configuration interactively (`kez uconf`), the system
prompts the user to choose a concrete implementation for each abstract package
from its `implementations` list.

When installing via command line (`kez install <package>`), the system uses
the advisor lookup table (`src/dependency_resolver/advisor.cpp`) for automatic
selection based on the target architecture:

```yaml
# heuristics/advice.yaml
advice:
  blas:
    x86_64: intel-oneapi-mkl
    arm64: nvpl
  mpi:
    x86_64: openmpi
    arm64: openmpi
```

### Selection Validation

The parser validates the user's selections against the database. If
`recipe.abstract_packages` contains an entry where the value is not a valid
implementation of the abstract package, parsing terminates with:
`'<implementation>' does not implement abstract package '<abstract_name>'`

See [Abstract Package Resolution](user-config-format.md#abstract-package-resolution)
in the User Configuration Format for how this interacts with user-editable configs.


## Configuration Guidelines

1. **Use a field only if it is necessary.** Omit optional sections that are not needed.
2. **Use default values.** Skip `conditions` if the default value suffices.
3. **Use conditions wisely.** Within a package, conditions must only depend on entries
   defined earlier in the same package. Conditions in `build` must not depend on
   non-leaf `properties` entries.
4. **Keep it simple.** Avoid unnecessary complexity.
5. **Write descriptions.** Every description field helps users understand the purpose
   of a configuration item.

## Notes

- **YAML anchors** (`&name` / `*name`) may be used within a recipe to reuse the same
  configuration block across multiple stages. See `database/ascot5/latest.yaml` for an
  example.
- **The `recipe:` root key** distinguishes database files from user configuration files,
  which use a `kez:` root key. Archived example configurations in `archive/` use the
  `kez:` format — see [User Configuration Format](user-config-format.md) for details.
