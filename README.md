# Fromager

Fromager is a user-space HPC package manager for GNU/Linux. It lets you build and install multiple versions and configurations of scientific software side-by-side without root access, without interfering with the system environment, and without fighting a container runtime.

## Why Fromager?

Traditional package managers (apt, yum) don't let you have two versions of HDF5 for two different applications. Tools like Spack or EasyBuild solve this but at the cost of steep learning curves and complex recipes. Fromager's goals are:

- **Simple configuration**: one YAML file per package in the database; one YAML file per install
- **Precise environment control**: shell environment is only modified when you explicitly ask (`fgr cellar enter`, `fgr compiler load`)
- **HPC-native**: built-in support for MPI implementations, compilers, and vendor toolchains (NVHPC, Intel oneAPI)
- **Fast C++ backend**: the parser and dependency resolver are compiled, not interpreted

## Quick Start

```bash
# 1. Clone the repo
git clone -c feature.manyFiles=true https://github.com/SkyWorld117/Fromager.git
cd Fromager

# 2. Point Fromager at a work directory with ample space
export FROMAGER_WORKDIR=/scratch/${USER}/.fromager

# 3. Source the environment (run this once per shell session; add to .bashrc)
source setup-env.sh

# 4. Bootstrap the internal toolchain (takes ~30-60 min; run on a compute node)
fgr init

# 5. Install a package from an example configuration
fgr install -r examples/openmpi.yaml
```

After `fgr init` completes, Fromager is fully self-contained — it uses its own GCC, CMake, and build tools regardless of what is installed on the cluster.

## Documentation

| Document | Description |
|---|---|
| [01 — Getting Started](docs/01-Getting_Started.md) | Installation, initialization, basic usage |
| [02 — Developer Overview](docs/02-Developer_Overview.md) | Source code structure and architecture |
| [03 — Database Configuration Format](docs/03-Database_Configuration_Format.md) | Full reference for `database/*.yaml` package entries |
| [04 — Abstract Configuration Format](docs/04-Abstract_Configuration_Format.md) | Interface packages like `mpi`, `blas` |
| [05 — Templating](docs/05-Templating.md) | Template variable syntax (`${pkg.prefix}` etc.) |
| [06 — User Configuration Format](docs/06-User_Configuration_Format.md) | The per-install YAML files you write and edit |
| [07 — Factories](docs/07-Factories.md) | Batch builds and benchmarking with `fgr rt` |
| [08 — CLI Reference](docs/08-CLI_Reference.md) | Every `fgr` command with examples |
| [09 — Adding a Package](docs/09-Adding_a_Package.md) | Step-by-step guide to adding a new package to the database |

## Known Issues

- No AMD GPU / ROCm support yet (no test hardware available)
- Dependencies have no version constraints — compatibility is the user's responsibility
- No Python package support (planned)

## Development Setup

Fromager uses `clang-format` via pre-commit hooks for consistent C++ style.

```bash
pip install pre-commit
pre-commit install
# Optional: format all existing files
pre-commit run --all-files
```

Build the C++ backend manually (requires yaml-cpp and argparse from `cellars/system`):

```bash
make          # release build
make all      # release + test tools
make clean
```

## Contributing

The project is maintained by Team RACKlette. See [TODO.md](TODO.md) for open tasks. All changes go through pull requests against the `main` branch. A PR must pass `test-01.sh` before merging.

If you want to take on a task, open an issue or contact @SkyWorld117 first to avoid duplicated work.
