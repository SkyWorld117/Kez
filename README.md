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

## Development Setup

To contribute code to Fromager, you'll need to set up pre-commit hooks for code formatting:

```bash
# Install pre-commit
pip install pre-commit

# Install the git hooks
pre-commit install

# (Optional) Run on all existing files
pre-commit run --all-files
```

## How to Contribute

At the moment, the repository is mainly maintained by Team RACKlette. If you want to contribute, please feel free to open an issue or a pull request.

For RACKlette members, please refer to the [TODO list](TODO.md) for the current tasks and their status. If you want to take on a task, please contact me (@SkyWorld117) first then update the TODO list accordingly to avoid duplicated work. The `main` branch is protected, any form of update or contribution should be done through pull requests.
