# Fromager

Fromager is an HPC-focused package manager for GNU/Linux systems.

Everything is at its very early stage, use with caution.

You can find more information in the [documentation](docs/01-Getting_Started.md).

## Why Fromager?

Traditional package managers often struggle with the complexities of HPC environments, where multiple versions of libraries and tools need to coexist. Cluster administrators often leave the users to manage these complexities themselves, creating the demand of a user-site package manager that can handle such complexities.

Although there are existing solutions such as Spack and EasyBuild, they miss one or more key features below:

- User-friendly configuration
- High maintainability for package developers
- High performance
- Precise shell (bash) environment management

Fromager achieves the points above by using a C++ backend, a database of YAML files that are easy to modify and extend, and a novel environment structure to achieve both efficiency and isolation.

## Known Issues

- No actual AMD GPU support (ROCm etc.) as I do not have access to such hardware for testing. 
- Dependencies currently have no version constraints, but we may just leave it to the user to ensure compatibility. 
- No Python package support yet, but it is planned for the future.

## TODO

- [ ] Add multilevel parsing for factory cheese tasting.
- [ ] Add a test function that separates all the dependencies and the target package.
- [ ] Add `state.yaml` for each cellar to allow installation of multiple packages.
- [ ] Add mkdocs integration.
