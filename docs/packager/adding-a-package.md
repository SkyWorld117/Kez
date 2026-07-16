# Adding a Package

This guide walks through adding a new package to the Kez database.

## 1. Choose a Name

Select a short, descriptive name (e.g., `zlib`, `hdf5`, `openmpi`). Use lowercase
with hyphens for multi-word names (`fftw3`, `intel-oneapi-mkl`).

## 2. Create the Recipe Directory

```bash
mkdir database/<package_name>/
```

Each package gets its own directory with a `latest.yaml` file. This format supports
future multiple-version entries.

## 3. Write the Recipe

Start with the minimal skeleton:

```yaml
---
recipe:
    name: <package_name>
    description: <brief_description>
    type: <system|compiler|mpi|vendor|package|external|abstract>
    toolchain: <autotools|cmake|make>

    source:
        type: <tarball|git|zip|script>
        releases:
            - version: <x.y.z>
              url: <download_url>
```

### Choose the type

- **`system`**: Core libraries and tools (zlib, gmp, make, git)
- **`compiler`**: Compiler installations (gcc, llvm)
- **`mpi`**: MPI implementations (openmpi, mpich)
- **`vendor`**: Vendor SDKs (cuda, intel-oneapi, nvhpc)
- **`package`**: User-facing applications (everything else)
- **`external`**: System-provided libraries (rdma-core, hcoll, slurm) — no source/build,
  only properties pointing to system paths configured in `config.yaml`

### Choose the toolchain

- **`autotools`**: For packages using `./configure && make`
- **`cmake`**: For packages using CMake
- **`make`**: For packages using plain `make` without `./configure`
- **Omit**: For packages with no standard build system — use `preprocessing`/`postprocessing`
  instead (vendor scripts, prebuilt binaries, abstract packages)

### Add dependencies

```yaml
dependencies:
    - zlib
    - hdf5
```

### Define build options

For Autotools packages:

```yaml
build:
    configurations:
        options:
            - name: enable-shared
              enabled:
                  default: true
```

For CMake packages:

```yaml
build:
    configurations:
        options:
            - name: BUILD_SHARED_LIBS
              description: Build shared libraries
              enabled:
                  default: 'ON'
```

### Define build stages

```yaml
build:
    stages:
        - target:           # Default target (null = toolchain default)
          multithreaded: true
        - target: install   # Install target
```

The default command produced for a stage depends on the package's
[`toolchain`](#choose-the-toolchain):

- **`autotools` / `make`**: `make -j{N}` → `make install`
- **`cmake`**: `cmake --build build --parallel {N}` → `cmake --install build`
- **No toolchain** (generic): No defaults. Stages must carry an explicit
  `configurations.command` to produce any output, or they are silently
  discarded. Use `preprocessing`/`postprocessing` or the top-level
  `configurations.command` instead.

### Add environment variables

```yaml
build:
    configurations:
        environment:
            - name: CFLAGS
              description: C compiler flags
              user_configurable: true
              default: -O3
```

### Add properties

```yaml
properties:
    lib: ${package.prefix}/lib
    include: ${package.prefix}/include
    libs:
        default: -l<mylib>
```

## 4. Test the Recipe

```bash
# Validate basic parsing (check just your new package)
kez dbcheck --only <package_name>

# Or validate the entire database
kez dbcheck

# Generate a user configuration
kez uconf <package_name>

# Generate and save a configuration for inspection
kez uconf <package_name> --save /tmp/test-config.yaml
```

If `kez dbcheck` reports errors, check:
- YAML syntax (missing `---`, incorrect indentation)
- Required fields (`name`, `type` are mandatory)
- Source URLs are reachable

## 5. Common Patterns

### Git-based package

```yaml
source:
    type: git
    url: https://github.com/example/repo.git
    releases:
        - version: 1.0.0
          tag: v1.0.0
```

### Prebuilt binaries (no toolchain)

```yaml
source:
    type: tarball
    releases:
        - version: 24.0.0
          url: https://example.com/pkg-24.0.0-linux-x64.tar.xz

build:
    postprocessing: cp -r ${source}/* ${package.prefix}/
```

### Package with `make` toolchain

```yaml
    toolchain: make
    build:
        stages:
            - target:
              configurations:
                  options:
                      - name: CC
                        enabled_value:
                            default: ${compiler.c}

                      - name: PREFIX
                        enabled_value:
                            default: ${package.prefix}
            - target: install
```

With `toolchain: make`, options are passed as environment variables to `make`
(e.g., `make CC=gcc PREFIX=/path`).

### Package with a custom build (no toolchain)

Without a toolchain, stages must carry explicit commands to produce any output:

```yaml
source:
    type: tarball
    releases:
        - version: 1.0.0
          url: https://example.com/package-1.0.0.tar.gz

build:
    preprocessing: ./configure --prefix=${package.prefix}
    stages:
        - target:
          configurations:
              command: make -j${KEZ_NPROC}
        - target: install
          configurations:
              command: make install
```

> **Warning:** Without the per-stage `configurations.command`, these stages would
> emit no shell commands — there are no toolchain defaults to fall back on. Use
> the [`toolchain`](#choose-the-toolchain) field when the package uses
> autotools, cmake, or plain make with standard conventions.

### Package with conditional options

```yaml
options:
    - name: enable-mpi
      description: Enable MPI support
      user_configurable: true
      enabled:
          default: false
      requires:
          - openmpi
```

### Environment variable with conditions

```yaml
environment:
    - name: CXXFLAGS
      user_configurable: true
      default: -O3
      conditions:
          - condition: openmpi enabled
            action: append
            value: -DHAVE_MPI
```
