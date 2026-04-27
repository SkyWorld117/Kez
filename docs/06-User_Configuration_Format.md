# User Configuration Format

A user configuration file (sometimes called a "wheel") is a YAML file you hand to `fgr install -r` to describe exactly what to build and how. Fromager generates a starting template with `fgr template <package>`, which you then customize. This document explains every field and how they interact.

## File Structure

A user configuration file has two top-level sections: `cheese` and `recipe`.

```yaml
cheese:
  <package_name>:
    ...
  <dependency_name>:
    ...

recipe:
  abstract_packages:
    <abstract_name>: <concrete_implementation>
  dependencies:
    - <dep1>
    - <dep2>
  targets:
    - <target_package>
```

**You should not add or remove entries from either section** — only change the values of fields that the database marks as `user_configurable: true`, plus `version` and `compiler` on any package.

---

## The `cheese` Section

Each key under `cheese` is a package name. The entry for each package lets you pin versions, choose a compiler, and override build options.

### `version`

The version to build. Omit (or keep as the generated default) to use the latest release listed in the database.

```yaml
cheese:
  openmpi:
    version: 5.0.9
```

### `compiler`

Which compiler to use for this specific package. Accepts:

- `system` — use the bootstrapped GCC from `cellars/system/` (the default)
- `<vendor>@<version>` — use a versioned compiler from `cellars/compilers/`, e.g. `gcc@13.2.0`

```yaml
cheese:
  conquest:
    version: 1.4
    compiler: gcc@13.2.0
```

If omitted, the package inherits the global `default_compiler` from `$FROMAGER_WORKDIR/config.yaml`.

### `build.configurations.options`

A list of build options to override. Only options that the database entry marks `user_configurable: true` should appear here. The generated template already includes these with their defaults pre-filled.

Each option entry has the following fields:

| Field | Required | Description |
|---|---|---|
| `name` | yes | The option name (matches the database entry exactly) |
| `description` | no | Human-readable label (kept from the template for reference) |
| `enabled` | yes | `true` to pass this option to the build system, `false` to omit it |
| `enabled_value` | yes | The value to use when enabled (use `~` if the option takes no value) |
| `disabled_value` | no | Value to pass when disabled (rarely needed; use `~` to pass nothing) |
| `requires` | no | List of optional dependency names; if any are absent, the option is disabled automatically |

Example — enabling a flag with a value, and disabling another:

```yaml
build:
  configurations:
    options:
      - name: --with-cuda
        description: Build with CUDA support
        enabled: true
        enabled_value: ${cuda.prefix}
        requires: [cuda]
      - name: --enable-memchecker
        description: Enable memory and buffer checks
        enabled: false
        enabled_value: ~
```

Template variables like `${cuda.prefix}` are resolved at build time. See [Templating](05-Templating.md) for the full list.

### `build.stages`

Override the make targets and their options for this package. Only relevant for packages that expose configurable stages. The generated template shows which stages are configurable.

```yaml
build:
  stages:
    - target: ~            # default make target (empty string)
      configurations:
        options:
          - name: COMPFLAGS
            enabled: true
            enabled_value: -O3 ${compiler.omp_flags}
    - target: install
```

---

## The `recipe` Section

### `abstract_packages`

Maps each abstract package interface to the concrete implementation you want to use. Abstract packages are things like `mpi`, `blas`, `lapack`, or `fftw3-api` — interfaces that several concrete packages can implement.

```yaml
recipe:
  abstract_packages:
    mpi: openmpi
    blas: intel-oneapi-mkl
    lapack: intel-oneapi-mkl
    scalapack: intel-oneapi-mkl
    fftw3-api: intel-oneapi-mkl
```

If your application has no abstract dependencies, set this to an empty map:

```yaml
recipe:
  abstract_packages: {}
```

See [Abstract Configuration Format](04-Abstract_Configuration_Format.md) for how abstract packages work.

### `dependencies`

The full list of packages to install, including the target package and all of its transitive dependencies. This list is generated automatically by `fgr template` and is in topological order (dependencies before dependents). You generally should not reorder it.

```yaml
recipe:
  dependencies:
    - openssl
    - hwloc
    - ucx
    - openmpi
    - conquest
```

### `targets`

The package(s) you actually want installed. When you run `fgr install -r config.yaml`, Fromager reads `targets` to determine the cellar type and install path. Everything in `dependencies` will be built, but `targets` tells Fromager which package is the primary goal.

```yaml
recipe:
  targets:
    - conquest
```

For most user workflows, `targets` contains exactly one package name.

---

## Complete Example

Below is a full user configuration for CONQUEST (a DFT code) using OpenMPI and Intel oneAPI MKL for linear algebra. This is a realistic example derived from `examples/conquest.yaml`.

```yaml
cheese:
  conquest:
    description: CONQUEST linear scaling DFT code
    version: 1.4
    compiler: system
    build:
      stages:
        - target: ~
          configurations:
            options:
              - name: COMPFLAGS
                description: Fortran compiler flags
                enabled: true
                enabled_value: -O3 -fallow-argument-mismatch ${compiler.omp_flags} ${blas.includes} ${lapack.includes} ${scalapack.includes} ${fftw3-api.includes} ${libxc.includes}
              - name: MULT_KERN
                description: Matrix multiplication kernel
                enabled: true
                enabled_value: default

  libxc:
    version: 7.0.0
    compiler: system
    build:
      configurations:
        options:
          - name: CFLAGS
            enabled: true
            enabled_value: -O3
          - name: FCFLAGS
            enabled: true
            enabled_value: -O3

  openmpi:
    description: Open Source Implementation of the Message Passing Interface
    version: 5.0.8
    compiler: system

  intel-oneapi-mkl:
    description: Intel oneAPI Math Kernel Library
    build:
      configurations:
        options:
          - name: fortran_format
            description: Fortran interface format [ gf | intel ]
            enabled: true
            enabled_value: gf
          - name: integer_size
            description: Integer size [ lp64 | ilp64 ]
            enabled: true
            enabled_value: lp64
          - name: multithreaded
            description: Enable multithreading
            enabled: false
            enabled_value: ~
          - name: mpi-family
            description: MPI implementation [ openmpi | intelmpi ]
            enabled: true
            enabled_value: openmpi

  intel-oneapi:
    version: 2025.1.0.666

recipe:
  abstract_packages:
    mpi: openmpi
    scalapack: intel-oneapi-mkl
    fftw3-api: intel-oneapi-mkl
    lapack: intel-oneapi-mkl
    blas: intel-oneapi-mkl
  dependencies:
    - conquest
    - libxc
    - openmpi
    - intel-oneapi-mkl
    - intel-oneapi
  targets:
    - conquest
```

To install this:

```bash
fgr install -r examples/conquest.yaml --cellar conquest-lp64
```
