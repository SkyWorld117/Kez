# CLI Reference

```
Kez - an HPC-focused package manager

Usage: kez <command> [options]
```

## Global Commands

### `kez init`

Initialize or refresh the Kez toolchain.

```
Usage: kez init [--refresh] [--use-distro-compiler]

  --refresh               Recreate the system environment
  --use-distro-compiler   Link the distribution compiler instead of building GCC
```

The `init` command bootstraps the Kez system environment by building a complete toolchain
(GCC, binutils, cmake, make, git, etc.) as specified in `manifest.yaml`. This is a
long-running process that should be run on a login or compute node.

- `--refresh`: Rebuilds the system environment from scratch.
- `--use-distro-compiler`: Skips building GCC and links against the distribution's compiler
  instead, significantly reducing init time.

---

### `kez update`

Update the source tree and rebuild Kez.

```
Usage: kez update [--with-system]

  --with-system  Refresh the system toolchain after rebuilding Kez
```

Pulls the latest changes from the Git repository and rebuilds the `kez` binary.
Use `--with-system` to also refresh the system environment (same as `kez init --refresh`).

---

### `kez dbcheck`

Parse and validate the package database.

```
Usage: kez dbcheck [--only <package>...]

  --only <package>...  Check only the listed packages instead of the entire database
```

Without options, reads every recipe in the database and reports any parsing errors.
With `--only`, validation is restricted to the named packages — useful for quickly
checking a new or modified recipe without scanning the full database.

**Examples:**

```bash
# Validate the entire database
kez dbcheck

# Check just one package
kez dbcheck --only zlib

# Check multiple specific packages
kez dbcheck --only zlib hdf5 openmpi
```

---

### `kez --version` / `kez -V`

Print the Kez version.

---

## Package Management

### `kez install`

Install packages into an environment.

```
Usage: kez install [options] <package>...
       kez install --read [options] <config.yaml>

Options:
  -r, --read             Treat the positional argument as a YAML file
  -d, --dry-run          Show the commands that would be executed
  -c, --config PATH=VAL  Override a generated configuration value
  -e, --env NAME         Target application environment
  -f, --force            Reinstall packages already recorded in state.yaml
  -S, --with-slurm       Run scripts/install.sh through sbatch
      --silence          Generate configuration without prompting
  -R, --rebuild PACKAGE  Rebuild a package and its dependents in the environment
```

Package-based installation is interactive by default. Use `--silence` for
non-interactive generation; this includes every available optional dependency
so its options can be addressed with `--config`, while preserving recipe default
values. Abstract packages are selected through `heuristics/advice.yaml`.

**Parallelism during installation** is controlled by two environment variables
set automatically by `setup-env.sh`:
- `KEZ_NPROC` — number of processors per package build (`make -jN`).
- `KEZ_NJOBS` — maximum number of packages to dispatch concurrently.

Both must be set to a positive integer.  See the [Getting Started](getting-started.md#configuring)
guide for the corresponding `config.yaml` settings.

The `--rebuild` flag scans the environment's state, computes the transitive closure
of packages that depend on `PACKAGE`, and reinstalls them in dependency order. It
automatically implies `--force` for the rebuild set.

**Examples:**

```bash
# Install a package into the active environment
kez install zlib

# Install into a specific environment
kez install -e my-env openmpi

# Generate a user configuration file, edit it, then install from it
kez uconf zlib --save my-config.yaml
# (edit my-config.yaml)
kez install --read my-config.yaml

# Dry run to see what would be executed
kez install -d fftw3

# Override a configuration option
kez install -c zlib.prefix=/custom/path zlib

# Force reinstallation
kez install -f gcc

# Rebuild zlib and everything that depends on it
kez install --rebuild zlib

# Rebuild in a specific environment (dry-run first to see what would change)
kez install --rebuild hdf5 -e my-simulation -d
```

---

### `kez uconf`

Generate or inspect a user configuration.

```
Usage: kez uconf <package>... [--save FILE] [--silence]
```

Generates a user-configurable YAML file for one or more packages. The generated
YAML includes all dependencies, build options, and environment variables that are
marked as `user_configurable: true` in the database.

- Without `--save`, prints the configuration to stdout.
- With `--save FILE`, writes the configuration to the specified file.
- Both output modes are interactive by default. Options are grouped by the
  optional package they require;
  enabling at least one option automatically includes that package. Use the
  arrow keys or `j`/`k` to move, Space to toggle, and Enter to confirm.
  Abstract-package implementations are selected afterward from a mutually
  exclusive list. Confirming that list without a selection uses the default
  from `heuristics/advice.yaml`.
- With `--silence`, every available optional dependency is included without
  changing its configuration defaults, and abstract implementations use the
  architecture advisor. This mode is suitable for scripts and agents.

---

### `kez info`

Show package metadata.

```
Usage: kez info <package> [--raw]
```

Displays metadata for a package from the database, including description, type,
dependencies, and available versions.

- `--raw`: Output the raw YAML node instead of a formatted summary.

---

## Environment Management

### `kez env`

Manage application environments.

```
Usage: kez env <create|remove|list|enter|exit|which|empty> [name]
```

Environments are isolated installation prefixes for user applications.
A package of type `package` must be installed into an environment.

**Subcommands:**

| Subcommand | Description |
|---|---|
| `create NAME` | Create a new application environment |
| `remove NAME` | Remove an application environment and its module file |
| `list` | List all application environments |
| `enter NAME` | Activate an environment (adds its `bin/` to `PATH`) |
| `exit` | Deactivate the current environment |
| `which` | Show the currently active environment |
| `empty NAME` | Remove all packages from an environment |

**Example flow:**

```bash
kez env create my-simulation
kez env activate my-simulation
kez install fftw3
kez install openmpi
kez env deactivate
```

---

### `kez compiler`

Manage installed compiler environments.

```
Usage: kez compiler <load|unload|list|which|remove> [name]
```

**Subcommands:**

| Subcommand | Description |
|---|---|
| `load NAME` | Load a compiler (sets `CC`, `CXX`, `FC` and adds its `bin/` to `PATH`) |
| `unload` | Unload the current compiler |
| `list` | List installed compilers |
| `which` | Show the currently loaded compiler |
| `remove NAME` | Remove a compiler and its module file |

Only one compiler can be loaded at a time.

---

### `kez mpi`

Manage installed MPI environments.

```
Usage: kez mpi <load|unload|list|which|remove> [name]
```

Same interface as `kez compiler`, but for MPI implementations.
Only one MPI can be loaded at a time.

**Subcommands:**

| Subcommand | Description |
|---|---|
| `load NAME` | Load an MPI environment |
| `unload` | Unload the current MPI |
| `list` | List installed MPI environments |
| `which` | Show the currently loaded MPI |
| `remove NAME` | Remove an MPI environment and its module file |

---

### `kez utilities`

Manage the shared utilities environment.

```
Usage: kez utilities <add|empty> [options]

Options for add:
  -r, --read             Treat the positional argument as a YAML file
  -d, --dry-run          Show the commands that would be executed
  -c, --config PATH=VAL  Override a generated configuration value
  -f, --force            Reinstall packages already recorded in state.yaml
  -S, --with-slurm       Run scripts/install.sh through sbatch
  -R, --rebuild PACKAGE  Rebuild a package and its dependents (not valid for utilities)
```

Utilities are packages installed into a shared prefix (not scoped to an application
environment). Use this for tools that should be available everywhere, like `htop` or `ripgrep`.

**Subcommands:**

| Subcommand | Description |
|---|---|
| `add [options] <package>...` | Install packages as shared utilities |
| `empty` | Remove all packages from the utilities environment |

---

## Batch Operations

### `kez factory`

Manage batch build and profiling factories.

```
Usage: kez factory <create|remove|list|enter|exit|which|build|run|summarize> [options]

Commands:
  create NAME        Create a factory
  remove NAME        Remove a factory
  list               List factories
  enter NAME         Select a factory in the current shell
  exit               Clear the selected factory
  which              Show the selected factory
  build              Build all recipe YAML files into factory buildspace
  run                Run runspace profiles
  summarize          Print lines matching profile summary regexes

Build options:
  -d, --dry-run      Show installation commands without executing them
  -f, --force        Reinstall packages already recorded in state.yaml
  -S, --with-slurm   Run scripts/install.sh through sbatch
```

See the [Factories documentation](../developer/factories.md) for a full description of
the factory system and runtime profile format.
