#pragma once

#include <yaml-cpp/yaml.h>

#include <database/config.hpp>
#include <filesystem>
#include <string>
#include <uconf_parser/user_config_parser.hpp>
#include <vector>

/**
 * @brief Resolve a named path from ``manifest.yaml`` relative to the work
 *        directory.
 *
 * Reads ``<KEZ_HOME>/manifest.yaml`` and looks up the ``paths.<name>`` key.
 * If the configured path is absolute it is returned as-is; otherwise it is
 * resolved relative to ``KEZ_WORKDIR``.
 *
 * @param name  The manifest path key (e.g. ``"applications"``, ``"compilers"``,
 *              ``"factories"``).
 * @return The absolute or resolved filesystem path.
 *
 * @warning Terminates the program if ``KEZ_HOME`` or ``KEZ_WORKDIR`` are not
 *          set, if the manifest is missing, or if the path key is undefined.
 */
std::filesystem::path configured_work_path(const std::string& name);

/**
 * @brief Compute the installation prefix for a set of target packages.
 *
 * Determines the installation path based on the common @ref PackageType of
 * the targets and the environment name:
 *   - @c Package targets → ``<applications_path>/<environment_name>``
 *   - @c Compiler targets → ``<compilers_path>/<name>-<version>``
 *   - @c MPI targets → ``<mpis_path>/<name>-<version>-<compiler>``
 *   - @c Vendor targets → ``<vendors_path>/<name>-<version>``
 *   - @c System targets → ``<system_path>``
 *   - @c Utilities (``utilities=true``) → ``<utilities_path>``
 *
 * The subdirectory paths are resolved via @ref configured_work_path.
 *
 * @param user_config       The parsed user configuration YAML node (used to
 *                          extract target package names and their versions).
 * @param environment_name  Name of the application environment (only used
 *                          when targets are of type @c Package).
 * @param utilities         If true, return the utility environment prefix
 *                          instead of a package-type-specific prefix.
 * @param renamed_version   Optional replacement for the version path component
 *                          of compiler, MPI, or vendor targets. This does not
 *                          modify the package version in @p user_config.
 * @return The absolute installation prefix path.
 *
 * @warning Terminates the program if the target packages have mixed types,
 *          if a compiler/MPI/vendor target is specified without a version,
 *          or if no environment name is supplied for @c Package targets.
 */
std::filesystem::path installation_prefix(const YAML::Node& user_config,
                                          const std::string& environment_name, bool utilities,
                                          const std::string& renamed_version = "");

/**
 * @brief Validate that a string is safe to use as a filesystem path component.
 *
 * Checks that `value` contains no forward slashes, null bytes, or whitespace
 * characters. If the check fails, the function prints an error message that
 * includes `description` and terminates the program via `exit(EXIT_FAILURE)`.
 *
 * @param value       The candidate path component to validate.
 * @param description A human-readable label for the value, used in the error
 *                    diagnostic (e.g. "environment name").
 */
void validate_path_component(const std::string& value, const std::string& description);

/**
 * @brief Execute an external command synchronously via the system shell.
 *
 * Invokes `std::system()` with the given command string. If the command
 * returns a non-zero exit status, the function terminates the program with
 * `exit(EXIT_FAILURE)`.
 *
 * @param command  The shell command string to execute.
 */
void run_external_command(const std::string& command);

/**
 * @brief Print all immediate subdirectories under a root path, preceded by a heading.
 *
 * Iterates over the entries in `root` and prints the filename of each
 * directory entry to stdout. The `heading` string is printed on a separate
 * line before the listing. Non-directory entries are silently skipped.
 *
 * @param root     The parent directory to scan.
 * @param heading  A header string printed before the directory listing.
 */
void list_directories(const std::filesystem::path& root, const std::string& heading);

/**
 * @brief Emit shell commands that set up an environment for activation.
 *
 * Iterates over every non-hidden subdirectory under @p prefix (mimicking
 * ``gen_modulefile.sh``) and prepends each package's ``bin/`` directory to
 * ``PATH``, ``share/man`` to ``MANPATH``, and ``lib/pkgconfig`` /
 * ``lib64/pkgconfig`` / ``share/pkgconfig`` to ``PKG_CONFIG_PATH``.
 * Finally, sets the marker @p variable to @p value.
 *
 * @param prefix    The environment root whose package subdirectories are
 *                  scanned (e.g. ``<work>/applications/<name>``).
 * @param variable  The environment variable name to set as an activation
 *                  marker (e.g. ``KEZ_ACTIVE_ENV``).
 * @param value     The value to assign to @p variable.
 */
void emit_environment_activation(const std::filesystem::path& prefix, const std::string& variable,
                                 const std::string& value);

/**
 * @brief Emit shell commands that undo an environment activation.
 *
 * Removes every ``bin/``, ``share/man/``, and pkg-config path that
 * emit_environment_activation() would have added from ``PATH``,
 * ``MANPATH``, and ``PKG_CONFIG_PATH``, then unsets the marker
 * @p variable.
 *
 * The output is intended to be evaluated by the calling shell (via ``eval``)
 * and is produced by ``main.sh`` when the user runs ``kez env deactivate``,
 * ``kez compiler unload``, or ``kez mpi unload``.
 *
 * @param prefix    The environment root whose package subdirectories are
 *                  scanned (same value passed to the corresponding
 *                  emit_environment_activation call).
 * @param variable  The marker variable to unset (e.g. ``"KEZ_ACTIVE_ENV"``,
 *                  ``"KEZ_COMPILER"``, ``"KEZ_MPI"``).
 */
void emit_environment_deactivation(const std::filesystem::path& prefix,
                                   const std::string& variable);

/**
 * @brief Print a human-readable summary of a BashCommandPlan to stdout.
 *
 * Iterates over every @ref PackageCommands entry in the plan and prints the
 * package name followed by each associated shell command on a separate line.
 * Used by the ``--dry-run`` mode of ``kez install`` to show what would be
 * executed without actually building anything.
 *
 * @param plan  The command plan whose contents should be printed.
 *
 * @see BashCommandPlan
 * @see parse_user_config
 */
void print_command_plan(const BashCommandPlan& plan);

/**
 * @brief Return a human-readable string for a PackageType enumerator.
 *
 * Maps each PackageType value to its canonical string representation:
 *   PackageType::Package  -> "package"
 *   PackageType::System   -> "system"
 *   PackageType::Compiler -> "compiler"
 *   PackageType::MPI      -> "mpi"
 *   PackageType::Vendor   -> "vendor"
 *   PackageType::Abstract -> "abstract"
 *   PackageType::External -> "external"
 *
 * @param type  The PackageType enumerator to convert.
 * @return A C-style string literal naming the type (never null).
 */
std::string package_type_name(PackageType type);

/**
 * @brief Extract the list of target package names from a user configuration.
 *
 * Reads the ``recipe.targets`` sequence from the user configuration YAML.
 * Each element must be a scalar string naming a target package.
 *
 * @param user_config  The parsed user configuration YAML node.
 * @return A vector of target package name strings.
 *
 * @warning Terminates the program if ``recipe.targets`` is missing, is not
 *          a sequence, or is empty.
 */
std::vector<std::string> user_config_targets(const YAML::Node& user_config);
