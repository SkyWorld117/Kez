# Cheese

## Structure of $CHEESE_WORKDIR

```
$CHEESE_WORKDIR
├── config.yaml
├── states.yaml
├── compilers.yaml
├── mpis.yaml
├── env
    ├── system
    ├── utilities
    ├── compilers
        ├── gcc-x.x.x
        ├── llvm-x.x.x
        ├── ...
    ├── mpis
        ├── openmpi-gcc-x.x.x-x.x.x
        ├── openmpi-llvm-x.x.x-x.x.x
        ├── ...
    ├── vendors
        ├── nvhpc-x.x
        ├── oneapi-x.x.x.x
        ├── ...
    ├── apps
        ├── ...
```

## Abstract Packages

Abstract packages offer exact implementations of a certain interface, such as `mpi` or `blas`. The exact implementation is not configurable due to the complexity of the interface, but some common parameters can still be configured. 

## Templating in configurations

Sometimes it is necessary to configure certain parameters dynamically based on the environment or other variables. Cheese uses a simple templating system for this purpose. The syntax is similar to Bash variable expansion, but with a few differences:
- Variables are enclosed in `${}` instead of `$()`.
- Variables are package-based, meaning they are prefixed with the package name followed by a dot, e.g., `${mpi.prefix}`.

In addition, there are some special variables that can be used:
- `compiler`: The currently selected compiler package, even though it may not appear in the dependencies.
- `mpi`: The currently selected MPI package if the package has an MPI dependency.
They offer not only the same variables as above, but also some additional ones:
- `${compiler.c}`: The C compiler binary.
- `${compiler.cxx}`: The C++ compiler binary.
- `${compiler.fort}`: The Fortran compiler binary.
- `${mpi.c}`: The MPI C compiler binary.
- `${mpi.cxx}`: The MPI C++ compiler binary.
- `${mpi.fort}`: The MPI Fortran compiler binary.

It is also possible use the current package name as a variable for self-reference, e.g., `${gcc.prefix}` when building the `gcc` package. This is useful for certain operations that require the package to refer to its own target directory or other properties.

Additionally, there is a special variable `${source}` that refers to the source directory of the package. This is used for manipulating files in the source directory for lateral use. 