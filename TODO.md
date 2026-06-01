# Core
| Status | Task Description | <div style="width:70px">Developer</div> |
|------------------|--------|-----------|
| Not Started | Use a class to wrap the database configurations to provide better type safety, more flexible querying, more default values and better error handling. | - |
| Started | Add more detailed unit tests for the C++ backend. | Bogdan |
| Not Started | Decide, document and unify the passes over the cheeses. | - |
| Not Started | When suggesting MPIs or compilers, prioritize existing installations on the system instead of always suggesting the latest version. | - |
| Started | Add caching system to store source files, reduce redundant downloads and rebuild without internet connection. | Nick |
| Done | Fix parsing regular environment variables (e.g. `PATH`, `LD_LIBRARY_PATH`) in the configuration. | Yi |
| Done | Add better architecture handling logic, the architecture type should not be exposed to the user. | Sophia |
| Done | Better source file handling (e.g. support for different source file name patterns). | Yi |
| Done | Fix CMake multithreading. (Use `cmake --build . --parallel` instead of `make -j`). | Nick |
| Done | Unify the path finding logic by using a C++ executable/header that can be called from both the CLI and the C++ backend. | Yi |
| Done | Use `export` for environment variables instead of initializing them in as "one-liner" before executing the command. | Yi |
| Done | Allow missing packages in the global configuration as long as they are not required by any package in the user configuration. | Yi |
| Done | Reduce the bash main script by moving some logic to the C++ backend. | Yi |
| Done | Add basic tests. | Yi |

# User Interface and Interaction Logic
| Status | Task Description | <div style="width:70px">Developer</div> |
|------------------|--------|-----------|
| Not Started | Add mkdocs integration. | - |
| Not Started | Improve templating documentation. | Nick |
| Not Started | Document the command line interface. | - |
| Not Started | Add a TUI interface for easier configuration and package management. | - |
| Not Started | Add short description for `info`. | Alex P. |
| Done | Add environment module interface to allow users to load the environment of a package with `module load <package>` after installation. | Yi |
| Done | Add toolchain based parsing and templating (e.g. `prefix` will be automatically set to `CMAKE_PREFIX_PATH` if the package is being built with CMake). | Yi |
| Done | Add an option to initialize Fromager with a scheduler (e.g. Slurm). | Yi |
| Done | Allow templating multiple packages at once. | Yi |
| Done | Allow rebuilding the whole cellar with a specific flag. | Yi |
| Done | Clean up debug printing to use macros. | Sophia & Yi |
| Done | Allow saving a template of multiple packages. | Yi |
| Done | Allow setting a default compiler in `config.yaml`. | Yi |
| Done | Allow overriding a user config with command line options. | Yi |
| Done | When `workdir` is not set, throw an error instead of using the default `~/.fromager` to avoid confusion. | Yi |
| Done | `wget` should throw an error when the download fails instead of silently continuing and causing build failures later. | Yi |

# Package Development and Configurations
| Status | Task Description | <div style="width:70px">Developer</div> |
|------------------|--------|-----------|
| Not Started | Allow installing multiple versions of CUDA (and ROCm if ever). | - |
| Not Started | Fix `acts` bad isolation. | - |
| Not Started | Update `exascale-climate-emulator` to the public GitHub version, possibly remove the sketchy `patchelf` logic. | - |
| Done | Add `neovim` package. | Yi |
| Done | Add `cmake` package. | Sophia |
| Not Started | Add `bear - build ear` package | Sophia |
| Done | Fix `gcc` wrong `stdc++` dynamic linking. | Yi |

# Uncategorized
| Status | Task Description | <div style="width:70px">Developer</div> |
|------------------|--------|-----------|
| Not Started | Add bash and yaml formatter. | - |
