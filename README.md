<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/assets/kez-logo-text-dark.svg">
  <source media="(prefers-color-scheme: light)" srcset="docs/assets/kez-logo-text.svg">
  <img src="docs/assets/kez-logo-text.svg" alt="Kez" width="250">
</picture>

Kez is an HPC-focused package manager for GNU/Linux on 64-bit x86 and ARM systems.

Everything is at its very early stage, use with caution.

You can find more information in the [documentation](docs/README.md).

## Why Kez?

Traditional package managers often struggle with the complexities of HPC environments, where multiple versions of libraries and tools must coexist. Cluster administrators often leave users to manage these complexities themselves, creating the need for a user-site package manager designed for this task.

Kez is built specifically for HPC, with the following key features:

- **Fast Backend**: Kez is written in C++ for performance, resolving large dependency trees in milliseconds.
- **Virtual Package Interfaces**: Declare abstract dependencies like MPI, BLAS, LAPACK, and FFTW in your recipes. Kez automatically selects the best available implementation per architecture based on built-in benchmarking, and you can override the choice at any time.
- **Isolated Environments with Shared Dependencies**: Every environment is self-contained to prevent library conflicts, while compilers, MPI stacks, and vendor SDKs are shared across packages — saving disk space and ensuring compatibility.
- **Environment Module Support**: Kez generates Tcl modulefiles compatible with Lmod, making it easy to switch between software stacks on shared clusters.
- **No Configuration Required**: Install software with a single command — Kez provides sensible defaults out of the box. Advanced users can generate and edit configuration files to fine-tune the build process when needed.
- **Transparent Builds**: Use `--dry-run` to inspect every command before it executes, and review a full build plan for any installation.
- **Simple YAML Recipes**: Package recipes are straightforward YAML — no Python DSL to learn. This makes creating and maintaining packages accessible to cluster administrators and researchers alike.
- **Dual-Level Build Parallelism**: Control per-package and inter-package concurrency independently to maximize hardware utilization when building large dependency graphs.
- **Developer-Friendly Mode**: Build and test software directly from a local source directory using pre-defined recipes — ideal for iterating on new versions before packaging.
- **Factory System for Batch Builds**: Create isolated build workspaces, run build profiles, and summarize results with regex matching — designed for testing packages across compiler and MPI variants at scale.

## Known Issues

- Stale ARM support (aarch64): The code path is ARM-aware, but the database might be outdated due to lack of testing on ARM hardware.
- No actual AMD GPU support (ROCm etc.): The database has almost no packages that support AMD GPUs due to lack of testing on AMD GPU hardware.
- No Python package support yet, but it is planned for the future.

## Development Setup

To contribute code to Kez, you'll need to set up pre-commit hooks for code formatting:

```bash
# Install pre-commit
pip install pre-commit

# Install the git hooks
pre-commit install

# (Optional) Run on all existing files
pre-commit run --all-files
```
Once set up, all C++ code will be automatically checked and formatted before each commit.
If your code doesn't match the formatting rules, the hook will format it for you. 
You'll then need to stage the formatted changes with 'git add' and commit again.

## How to Contribute

At the moment, the repository is mainly maintained by Team RACKlette. If you want to contribute, please feel free to open an issue or a pull request.

The `main` branch is protected, any form of update or contribution should be done through pull requests.

A pull request should pass the test suite (`make test`) and should ideally be tested on a real HPC cluster before merging. The test suite covers the C++ backend components including database parsing, dependency resolution, user config generation, and command-line parsing.

### Database Contributions

Database contributions are welcome! If you want to add a new package, please follow the existing structure in the `database` folder. Each package should have its own YAML file with the necessary metadata, dependencies, and build instructions. Make sure to test your package on a real HPC cluster and ensure that it builds and installs correctly. Notice that **ALL** the dependencies of the package should be present in the database, and the package is tested with all of them. Otherwise, the package will be rejected.

### AI Contributions

Yes, we are open to AI contributions! However, you need to follow the guidelines below:

- A natural human must be responsible for the final code submission.
- The AI-generated code should be reviewed and tested by a human before submission. If the corresponding human cannot answer questions about the code, it will be rejected.
- The AI-generated code should be short and simple, because we review each pull request manually and we cannot spend too much time on reviewing complex code.

## Disclaimer

As a package manager, Kez downloads and builds software from third-party sources. When you install software via Kez, you are responsible for complying with that software's license, and for any issues that may arise from using it. The Kez team is not responsible for any damage or loss caused by the use of Kez or the software it manages. Use at your own risk.
