# Core
| Status | Task Description | <div style="width:70px">Developer</div> |
|------------------|--------|-----------|
| Not Started | Add caching system to store source files, reduce redundant downloads and rebuild without internet connection. | - |
| Not Started | Better source file handling (e.g. support for different source file name patterns). | - |
| Done | Fix CMake multithreading. (Use `cmake --build . --parallel` instead of `make -j`). | Nick |
| Not Started | Unify the path finding logic by using a C++ executable/header that can be called from both the CLI and the C++ backend. | - |
| Not Started | Add `state.yaml` for compilers and MPIs to indicate which compilers and MPIs are available. | - |
| Done | Use `export` for environment variables instead of initializing them in as "one-liner" before executing the command. | Yi |
| Not Started | Add better architecture handling logic, the architecture type should not be exposed to the user. | Sophia |
| Done | Allow missing packages in the global configuration as long as they are not required by any package in the user configuration. | Yi |
| Not Started | Reduce the bash main script by moving some logic to the C++ backend. | Yi |
| Done | Add basic tests. | Yi |

# User Interface and Interaction Logic
| Status | Task Description | <div style="width:70px">Developer</div> |
|------------------|--------|-----------|
| Not Started | Add mkdocs integration. | - |
| Not Started | Improve templating documentation. | Nick |
| Not Started | Allow rebuilding the whole cellar with a specific flag. | - |
| Not Started | Allow saving a template of multiple packages. | - |
| Not Started | Allow templating multiple packages at once. | - |
| Not Started | Document the command line interface. | - |
| Not Started | Allow setting a default compiler in `config.yaml`. | - |
| Not Started | Add toolchain based parsing and templating (e.g. `prefix` will be automatically set to `CMAKE_PREFIX_PATH` if the package is being built with CMake). | - |
| Not Started | Add an option to initialize Fromager with a scheduler (e.g. Slurm). | - |
| Not Started | Allow overriding a user config with command line options. | - |
| Done | When `workdir` is not set, throw an error instead of using the default `~/.fromager` to avoid confusion. | Yi |
| Not Started | Add environment module interface to allow users to load the environment of a package with `module load <package>` after installation. | - |
| Done | `wget` should throw an error when the download fails instead of silently continuing and causing build failures later. | Yi |
| Not Started | Add a TUI interface for easier configuration and package management. | - |

# Package Development and Configurations
| Status | Task Description | <div style="width:70px">Developer</div> |
|------------------|--------|-----------|
| Not Started | Fix `acts` bad isolation. | - |
| Not Started | Update `exascale-climate-emulator` to the public GitHub version, possibly remove the sketchy `patchelf` logic. | - |
| Done | Fix `gcc` wrong `stdc++` dynamic linking. | Yi |
| Not Started | Add `bear - build ear` package | Sophia |

# Uncategorized
| Status | Task Description | <div style="width:70px">Developer</div> |
|------------------|--------|-----------|
