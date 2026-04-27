# CLI Reference

All commands are invoked through the `fgr` shell function, which is sourced into your shell by `setup-env.sh`. A subset of commands (`cellar enter/exit`, `compiler load/unload`, `mpi load/unload`, `rt factory enter/exit`) modify shell environment variables and must go through `fgr` rather than calling `bin/fromager` directly.

---

## `fgr init`

Bootstrap the Fromager toolchain. Builds a self-contained GCC compiler, GNU build system, CMake, and all Fromager dependencies into `cellars/system/`, then compiles the Fromager binary itself.

This command must be run once before any other `fgr` command. It takes significant time; run it on a compute node.

```
fgr init
```

---

## `fgr selfcheck`

Validate every YAML file in the database for format correctness. Useful after adding or modifying a package entry.

```
fgr selfcheck
```

---

## `fgr update`

Pull the latest package database from the upstream repository.

```
fgr update
```

---

## `fgr install`

Install one or more packages.

```
fgr install <package> [<package>...] [options]
fgr install -r <config.yaml> [options]
```

**Arguments:**

| Argument | Description |
|---|---|
| `<package>` | Package name(s) from the database |
| `-r`, `--read <file>` | Read a user configuration YAML file instead of a bare package name |

**Options:**

| Option | Description |
|---|---|
| `-c`, `--config <key=val>...` | Override build options on the command line (e.g. `version=5.0.9`) |
| `-C`, `--cellar <name>` | Target cellar for non-compiler/MPI/vendor packages |
| `-f`, `--force` | Reinstall even if the package is already recorded in `state.yaml` |

**How the cellar is determined:**

- **Compiler packages** (`type: compiler`) always install to `cellars/compilers/<name>-<version>/`.
- **MPI packages** (`type: mpi`) always install to `cellars/mpis/<name>-<version>-<compiler-spec>/`.
- **Vendor packages** (`type: vendor`) always install to `cellars/vendors/<name>-<version>/`.
- **All other packages** install to the cellar specified by `--cellar`. If omitted, the cellar is derived from the package name.

**Examples:**

```bash
# Install GCC 13.2.0 using a command-line version override
fgr install gcc --config version=13.2.0

# Install OpenMPI from a user config file
fgr install -r examples/openmpi.yaml

# Install CONQUEST into a named cellar
fgr install -r examples/conquest.yaml --cellar conquest-mkl

# Force-reinstall a package that was already installed
fgr install -r examples/openmpi.yaml --force
```

---

## `fgr template`

Generate a user configuration YAML template for a package and its dependencies. The template is printed to stdout; redirect it to a file to save it.

```
fgr template <package>
```

**Example:**

```bash
fgr template conquest > my-conquest.yaml
# Edit my-conquest.yaml to select versions and options, then:
fgr install -r my-conquest.yaml --cellar conquest-run1
```

The generated file includes all user-configurable options with their defaults and descriptions. See [User Configuration Format](06-User_Configuration_Format.md) for how to interpret and edit it.

---

## `fgr cellar`

Manage application-specific isolated environments.

### `fgr cellar create <name>`

Create a new empty cellar directory.

```bash
fgr cellar create conquest-v1
```

Reserved names (`system`, `compilers`, `mpis`, `vendors`, `utilities`) cannot be used.

### `fgr cellar remove <name>`

Delete a cellar and all its installed packages.

```bash
fgr cellar remove old-cellar
```

### `fgr cellar list`

List all user-created cellars (does not show reserved cellars).

```bash
fgr cellar list
```

### `fgr cellar enter <name>`

Add the cellar's `bin/` to `$PATH` and set `$FROMAGER_CELLAR`. Required before using `fgr rt try`.

```bash
fgr cellar enter conquest-v1
```

### `fgr cellar exit`

Remove the current cellar's `bin/` from `$PATH` and unset `$FROMAGER_CELLAR`.

```bash
fgr cellar exit
```

### `fgr cellar which`

Print the name of the currently entered cellar, or a message if none is active.

```bash
fgr cellar which
```

### `fgr cellar empty <name>`

Delete all contents of a cellar without deleting the cellar directory itself. Useful for a clean rebuild without having to recreate the cellar.

```bash
fgr cellar empty conquest-v1
```

---

## `fgr compiler`

Manage versioned compiler installations in `cellars/compilers/`.

### `fgr compiler load <name>`

Add a compiler's `bin/` to `$PATH` and set `$FROMAGER_COMPILER`. The `<name>` is the directory name under `cellars/compilers/`, e.g. `gcc-13.2.0`.

```bash
fgr compiler load gcc-13.2.0
```

### `fgr compiler unload`

Remove the currently loaded compiler from `$PATH` and unset `$FROMAGER_COMPILER`.

```bash
fgr compiler unload
```

### `fgr compiler list`

List all installed compilers.

```bash
fgr compiler list
```

### `fgr compiler which`

Print the name of the currently loaded compiler.

```bash
fgr compiler which
```

### `fgr compiler remove <name>`

Delete a compiler installation permanently.

```bash
fgr compiler remove gcc-11.5.0
```

---

## `fgr mpi`

Manage MPI implementations in `cellars/mpis/`. MPI directory names encode the compiler they were built against (e.g. `openmpi-5.0.9-gcc-13.2.0`), allowing multiple builds of the same MPI version to coexist.

### `fgr mpi load <name>`

Add an MPI's `bin/` to `$PATH` and set `$FROMAGER_MPI`.

```bash
fgr mpi load openmpi-5.0.9-gcc-13.2.0
```

### `fgr mpi unload`

Remove the currently loaded MPI from `$PATH` and unset `$FROMAGER_MPI`.

```bash
fgr mpi unload
```

### `fgr mpi list`

List all installed MPI implementations.

```bash
fgr mpi list
```

### `fgr mpi which`

Print the name of the currently loaded MPI implementation.

```bash
fgr mpi which
```

### `fgr mpi remove <name>`

Delete an MPI installation permanently.

```bash
fgr mpi remove openmpi-5.0.8-gcc-13.2.0
```

---

## `fgr rt`

Rapid-testing (rt) commands for batch builds and benchmarking. See [Factories](07-Factories.md) for the full factory workflow.

### `fgr rt factory create <name>`

Create a new factory directory with the expected subdirectory layout (`wheels/`, `tasting_rooms/`).

```bash
fgr rt factory create perf-sweep-2025
```

### `fgr rt factory remove <name>`

Delete a factory and all its contents.

```bash
fgr rt factory remove old-sweep
```

### `fgr rt factory list`

List all factories.

```bash
fgr rt factory list
```

### `fgr rt factory enter <name>`

Set `$FROMAGER_FACTORY` to the named factory. Required before running `fgr rt build`, `taste`, or `summarize`.

```bash
fgr rt factory enter perf-sweep-2025
```

### `fgr rt factory exit`

Unset `$FROMAGER_FACTORY`.

```bash
fgr rt factory exit
```

### `fgr rt factory which`

Print the currently entered factory name.

```bash
fgr rt factory which
```

### `fgr rt build`

Build all user configuration files found in the active factory's `wheels/` directory. Each `.yaml` file becomes a separate cellar under `factory/<name>/cellar/<stem>/`.

Must be run while inside a factory (`fgr rt factory enter <name>`).

```bash
fgr rt factory enter perf-sweep-2025
fgr rt build
```

### `fgr rt taste`

Run all benchmark profiles defined in the active factory's `tasting_rooms/config.yaml`. Results are written to subdirectories of `tasting_rooms/`.

```bash
fgr rt taste
```

### `fgr rt summarize`

Post-process tasting room output and produce a summary report.

```bash
fgr rt summarize
```

### `fgr rt try <config.yaml> <package>`

Build and immediately run a single package in debug mode inside the currently entered cellar. Useful for quickly iterating on a build configuration without creating a full factory.

Must be run while inside a cellar (`fgr cellar enter <name>`).

```bash
fgr cellar enter my-cellar
fgr rt try examples/conquest.yaml conquest
```
