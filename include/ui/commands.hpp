#pragma once

#include <string>
#include <vector>

/**
 * @brief Type alias for the arguments passed to a command handler.
 *
 * Represents the sequence of command-line tokens that follow the subcommand
 * name in a `kez <subcommand> [args...]` invocation.  For example, in
 * `kez install --dry-run zlib hdf5`, the vector contains `["--dry-run", "zlib",
 * "hdf5"]`.
 */
using CommandArguments = std::vector<std::string>;

/**
 * @brief Initialize or refresh the Kez system toolchain.
 *
 * Locates and runs `scripts/init.sh` under @c KEZ_HOME, which bootstraps a
 * distro-independent system stack.  The system environment is created on first
 * run and can be recreated with `--force`.
 *
 * @param arguments  Command-line tokens after the `init` subcommand.
 *                   Supported options:
 *                   - `--force`                : Recreate the system environment
 *                   - `--use-distro-compiler`  : Link the distribution compiler
 *                                                instead of building GCC
 *                   - `-h` / `--help`          : Print usage and return
 *
 * @note Terminates the process with an error if `scripts/init.sh` is missing
 *       or an unknown option is provided.
 *
 * @see execute_update
 */
void execute_init(const CommandArguments& arguments);

/**
 * @brief Update the source tree and rebuild Kez.
 *
 * Performs a `git pull --ff-only` inside @c KEZ_HOME, then rebuilds the
 * project with `make -B`.  When `--with-system` is passed, the system
 * toolchain is refreshed afterwards via `scripts/init.sh --force`.
 *
 * @param arguments  Command-line tokens after the `update` subcommand.
 *                   Supported options:
 *                   - `--with-system`  : Refresh the system toolchain after
 *                                        rebuilding
 *                   - `-h` / `--help`  : Print usage and return
 *
 * @note The environment variable @c KEZ_NPROC controls the number of parallel
 *       make jobs; it must be a positive integer.  The environment variable
 *       @c KEZ_NJOBS controls the maximum number of packages to build in
 *       parallel during installation and must be set in the environment.
 *
 * @see execute_init
 */
void execute_update(const CommandArguments& arguments);

/**
 * @brief Install packages into an application environment.
 *
 * Generates a user configuration from the requested packages, resolves
 * dependencies, parses the configuration into an executable install plan, and
 * runs `scripts/install.sh` to carry out the installation.  Supports reading
 * a pre-existing YAML configuration file (`--read`), dry-run mode
 * (`--dry-run`), config-value overrides (`--config`), and Slurm submission
 * (`--with-slurm`). For compiler, MPI, and vendor targets, `--rename` replaces
 * only the version component of the installation prefix.
 *
 * The `--rebuild` option recomputes the transitive closure of a target
 * package's dependents and reinstalls only that subset within an already-
 * populated environment.  When combined with `--read`, the user-provided
 * config file is used instead of an auto-generated one, allowing changes
 * to a custom config to flow through to the rebuilt packages.
 *
 * @param arguments  Command-line tokens after the `install` subcommand.
 *                   Supported options:
 *                   - `-r` / `--read FILE`   : Read config from a YAML file
 *                   - `-d` / `--dry-run`     : Show commands without executing
 *                   - `-f` / `--force`       : Reinstall packages already in
 *                                              state.yaml
 *                   - `-S` / `--with-slurm`  : Run install.sh through sbatch
 *                   - `--silence`             : Generate configuration without
 *                                              terminal prompts
 *                   - `-e` / `--env NAME`    : Target application environment
 *                   - `-c` / `--config PATH=VAL` : Override a config value
 *                   - `-R` / `--rebuild PKG` : Rebuild a package and its
 *                                              dependents
 *                   - `--rename NAME`         : Rename the managed-prefix
 *                                              version component
 *                   - `-h` / `--help`        : Print usage and return
 *
 * @note At least one package name (or `--read` with a file path) is required.
 *       The `--rebuild` and `--env` flags are mutually exclusive with the
 *       utility installation path.
 *
 * @see execute_utilities
 * @see execute_uconf
 */
void execute_install(const CommandArguments& arguments);

/**
 * @brief Manage the shared utilities environment.
 *
 * Supports three sub-actions:
 * - `add`   : Install one or more packages into the shared utilities directory
 *             (delegates to the same install logic as @c execute_install).
 * - `remove`: Delete a single utility package and remove it from state.yaml.
 * - `empty` : Remove all packages from the utilities environment.
 *
 * @param arguments  Command-line tokens after the `utilities` subcommand.
 *                   The first token must be `add`, `remove`, or `empty`.
 *                   For `add`, subsequent tokens are treated as install
 *                   options (see @c execute_install) followed by package names.
 *                   `remove` requires exactly one package name.
 *                   - `-h` / `--help` : Print usage and return
 *
 * @note The `--rebuild`, `--env`, and `--read` install options are not valid
 *       in the utilities context.
 *
 * @see execute_install
 */
void execute_utilities(const CommandArguments& arguments);

/**
 * @brief Generate or inspect a user configuration for one or more packages.
 *
 * Produces a YAML configuration document for the named packages by running
 * the user-config generation pipeline.  If `--save PATH` is given, the
 * configuration is written to the specified file; otherwise it is printed to
 * stdout.
 *
 * @param arguments  Command-line tokens after the `uconf` subcommand.
 *                   The first positional token(s) are package names (at least
 *                   one is required).
 *                   Supported options:
 *                   - `-s` / `--save FILE` : Write output to FILE instead of
 *                                            stdout
 *                   - `--silence`           : Generate without terminal prompts
 *                   - `-h` / `--help`      : Print usage and return
 *
 * @note Generation is interactive by default whether output is printed or
 *       saved. @c --silence includes every available optional dependency while
 *       retaining recipe defaults and selects abstract implementations through
 *       the architecture advisor.
 *
 * @see execute_install
 * @see gen_user_config
 */
void execute_uconf(const CommandArguments& arguments);

/**
 * @brief Manage application environments.
 *
 * Handles the lifecycle of named application environments under
 * `KEZ_WORKDIR/applications/`.  Supported actions:
 * - `create NAME` : Create a new empty environment directory
 * - `remove NAME` : Delete an environment and its modulefile
 * - `list`        : List all existing environments
 * - `enter NAME`  : Activate an environment in the current shell
 * - `exit`        : Deactivate the current environment
 * - `which`       : Show the currently active environment
 * - `empty NAME`  : Remove all packages from an environment while keeping
 *                   the directory
 *
 * @param arguments  Command-line tokens after the `env` subcommand.
 *                   The first token is the action; `create`, `remove`, `enter`,
 *                   and `empty` require a second token giving the environment
 *                   name.
 *                   - `-h` / `--help` : Print usage and return
 *
 * @note Shell-affecting commands (`enter`, `exit`) emit shell-evaluated output
 *       that must be consumed by the bash wrapper in @c main.sh.
 *
 * @see execute_compiler
 * @see execute_mpi
 */
void execute_environment(const CommandArguments& arguments);

/**
 * @brief Manage installed compiler environments.
 *
 * Handles loading, unloading, listing, querying, and removing compiler
 * environments under `KEZ_WORKDIR/compilers/`.  Supported actions:
 * - `load NAME`   : Activate a compiler in the current shell
 * - `unload`      : Deactivate the current compiler
 * - `list`        : List all installed compilers
 * - `which`       : Show the currently loaded compiler
 * - `remove NAME` : Delete a compiler installation and its modulefile
 *
 * @param arguments  Command-line tokens after the `compiler` subcommand.
 *                   The first token is the action; `load` and `remove` require
 *                   a second token giving the compiler name.
 *                   - `-h` / `--help` : Print usage and return
 *
 * @note Shell-affecting commands (`load`, `unload`) emit shell-evaluated
 *       output consumed by the bash wrapper in @c main.sh.  Only one compiler
 *       may be loaded at a time.
 *
 * @see execute_environment
 * @see execute_mpi
 */
void execute_compiler(const CommandArguments& arguments);

/**
 * @brief Manage installed MPI environments.
 *
 * Handles loading, unloading, listing, querying, and removing MPI environments
 * under `KEZ_WORKDIR/mpis/`.  Supported actions:
 * - `load NAME`   : Activate an MPI environment in the current shell
 * - `unload`      : Deactivate the current MPI environment
 * - `list`        : List all installed MPI environments
 * - `which`       : Show the currently loaded MPI environment
 * - `remove NAME` : Delete an MPI installation and its modulefile
 *
 * @param arguments  Command-line tokens after the `mpi` subcommand.
 *                   The first token is the action; `load` and `remove` require
 *                   a second token giving the MPI environment name.
 *                   - `-h` / `--help` : Print usage and return
 *
 * @note Shell-affecting commands (`load`, `unload`) emit shell-evaluated
 *       output consumed by the bash wrapper in @c main.sh.  Only one MPI
 *       environment may be loaded at a time.
 *
 * @see execute_environment
 * @see execute_compiler
 */
void execute_mpi(const CommandArguments& arguments);

/**
 * @brief List or remove installed vendor environments.
 *
 * Manages vendor installations under `KEZ_WORKDIR/vendors/`. Supported
 * actions:
 * - `list`        : List all installed vendors
 * - `remove NAME` : Delete a vendor installation and its modulefile
 *
 * @param arguments  Command-line tokens after the `vendor` subcommand.
 *                   `remove` requires a second token giving the vendor name.
 *                   - `-h` / `--help` : Print usage and return
 *
 * @see execute_compiler
 * @see execute_mpi
 */
void execute_vendor(const CommandArguments& arguments);

/**
 * @brief Show metadata for a single package from the database.
 *
 * Displays the package's name, description, author, type, toolchain,
 * available releases, implementations, dependencies, and configurable
 * properties.  With `--raw`, the raw YAML recipe file is printed verbatim.
 *
 * @param arguments  Command-line tokens after the `info` subcommand.
 *                   The first token is the package name (required).
 *                   Supported options:
 *                   - `-r` / `--raw` : Print the raw YAML recipe instead of
 *                                      the formatted summary
 *                   - `-h` / `--help` : Print usage and return
 *
 * @note Terminates with an error if the package is not found in the database.
 *
 * @see execute_dbcheck
 */
void execute_info(const CommandArguments& arguments);

/**
 * @brief Parse and validate the package database.
 *
 * Without options, iterates over every package directory under @c KEZ_DB,
 * parses each YAML configuration (including non-latest version files), and
 * validates version-range selection and overlap.  When @c --only is given,
 * validation is restricted to the named packages.
 *
 * @param arguments  Command-line tokens after the `dbcheck` subcommand.
 *                   Supported options:
 *                   - `--only <package>...` : Check only the listed packages
 *                   - `-h` / `--help`       : Print usage and return
 *
 * @note Terminates with an error if @c KEZ_DB does not exist or if any
 *       configuration fails to parse.
 *
 * @see execute_info
 * @see get_db_config
 * @see parse_db_config
 */
void execute_dbcheck(const CommandArguments& arguments);

/**
 * @brief Manage batch build and profiling factories.
 *
 * Factories are build-system profiles that describe how to compile and
 * profile packages in batch or on compute nodes.  Supported actions:
 * - `create NAME`      : Create a new factory
 * - `remove NAME`      : Delete a factory
 * - `list`             : List all factories
 * - `enter NAME`       : Select a factory in the current shell
 * - `exit`             : Clear the selected factory
 * - `which`            : Show the selected factory
 * - `build` [options]  : Build all recipe YAML files into the factory
 *                        buildspace
 * - `run` [options]    : Run runspace profiles
 * - `summarize`        : Print lines matching profile summary regexes
 *
 * Build/run options:
 * - `-d` / `--dry-run`      : Show commands without executing
 * - `-f` / `--force`        : Reinstall packages already in state.yaml
 * - `-S` / `--with-slurm`   : Run install.sh through sbatch
 *
 * @param arguments  Command-line tokens after the `factory` subcommand.
 *                   The first token is the action; `create` and `remove`
 *                   require a second token giving the factory name.
 *                   - `-h` / `--help` : Print usage and return
 *
 * @note Shell-affecting commands (`enter`, `exit`) emit shell-evaluated
 *       output consumed by the bash wrapper in @c main.sh.
 */
void execute_factory(const CommandArguments& arguments);
