# Kez Documentation

Kez is an HPC-focused package manager for GNU/Linux systems. Choose your path:

---

## 👤 User — I want to install and use packages

| Guide | Description |
|---|---|
| [Getting Started](user/getting-started.md) | Install, configure, initialize, and uninstall Kez |
| [CLI Reference](user/cli-reference.md) | All commands with options and examples |
| [Environments](user/getting-started.md#usage) | Application environments, compilers, MPI, utilities |

## 📦 Packager — I want to add or maintain package recipes

| Guide | Description |
|---|---|
| [Recipe Schema](packager/recipe-schema.md) | Full YAML format for database entries |
| [User Configuration Format](packager/user-config-format.md) | Format of the generated YAML that users edit |
| [Templating](packager/templating.md) | `${}` template syntax reference |
| [Adding a Package](packager/adding-a-package.md) | Step-by-step walkthrough for new packages |

## 🔧 Developer — I want to work on Kez itself

| Guide | Description |
|---|---|
| [Architecture](developer/architecture.md) | Pipeline, component map, code layout, `manifest.yaml` |
| [Build System](developer/build-system.md) | Makefile targets, test suite, formatting, contributing |
| [Factories](developer/factories.md) | Batch build and profiling system |

---

See also: [README](../README.md) (project overview), [AGENTS.md](../AGENTS.md) (AI coding guidelines).
