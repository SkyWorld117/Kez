#pragma once

#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <string>
#include <uconf_parser/user_config_parser.hpp>
#include <vector>

/**
 * @brief Apply command-line configuration overrides to a user config YAML node.
 *
 * Parses a list of `-D`-style configuration options (e.g. `OPTION=VALUE`) and
 * applies them directly to the in-memory YAML representation of the user
 * configuration.  The node is modified in place.
 *
 * @param user_config    The user configuration YAML node to modify.  Since
 *                       YAML::Node is a reference-counted handle, this
 *                       parameter acts as an in-out argument even though it is
 *                       passed by value; the caller's node is updated.
 * @param config_options A vector of strings of the form `OPTION=VALUE`
 *                       representing the configuration overrides supplied on
 *                       the command line.
 *
 * @warning Terminates the program with an error message if any configuration
 *          option path cannot be resolved in the YAML tree.
 *
 * @see parse_cmdline, which optionally calls this function internally when
 *      config_options is non-empty.
 */
void apply_cmdline_config(YAML::Node user_config, const std::vector<std::string>& config_options);

/**
 * @brief Parse a command-line configuration file into an executable build plan.
 *
 * Reads the YAML configuration from the given file path, optionally overlays
 * command-line configuration overrides via apply_cmdline_config, and
 * transforms the resulting configuration into a BashCommandPlan -- a list of
 * packages with their associated shell commands and dependencies.  The install
 * prefix determines where packages will be installed.
 *
 * @param file           Filesystem path to the user configuration YAML file
 *                       (typically `config.yaml`).
 * @param install_prefix The root directory under which all packages will be
 *                       installed.  This is forwarded to the user-config
 *                       parser as the primary installation prefix.
 * @param config_options Optional command-line `-D` overrides forwarded to
 *                       apply_cmdline_config.  Defaults to an empty vector.
 *
 * @return A BashCommandPlan (std::vector<PackageCommands>) containing the
 *         ordered list of packages to build together with their shell command
 *         sequences and inter-package dependencies.
 *
 * @note The returned plan is ready to be serialised with write_install_plan
 *       and then executed by scripts/install.sh.
 *
 * @see parse_cmdline(const std::vector<std::string>&, ...)
 * @see parse_cmdline(YAML::Node, ...)
 * @see write_install_plan
 */
BashCommandPlan parse_cmdline(const std::filesystem::path& file,
                              const std::filesystem::path& install_prefix,
                              const std::vector<std::string>& config_options = {});

/**
 * @brief Parse a list of target package names into an executable build plan.
 *
 * Instead of reading a configuration file, this overload accepts a flat list
 * of target package names and builds a non-interactive plan around them. The
 * targets are resolved through the YAML database (package recipes in
 * `database/`), available optional dependencies are retained with their recipe
 * defaults, and the result is returned as a BashCommandPlan.
 *
 * @param targets        A vector of package names (e.g. `{"gcc", "openmpi"}`)
 *                       that the user wants to install.
 * @param install_prefix The root directory under which all packages will be
 *                       installed.
 * @param config_options Optional command-line `-D` overrides that are merged
 *                       into the generated configuration.  Defaults to an
 *                       empty vector.
 *
 * @return A BashCommandPlan (std::vector<PackageCommands>) containing the
 *         ordered list of packages to build together with their shell command
 *         sequences and inter-package dependencies.
 *
 * @see parse_cmdline(const std::filesystem::path&, ...)
 * @see parse_cmdline(YAML::Node, ...)
 * @see write_install_plan
 */
BashCommandPlan parse_cmdline(const std::vector<std::string>& targets,
                              const std::filesystem::path& install_prefix,
                              const std::vector<std::string>& config_options = {});

/**
 * @brief Parse an in-memory YAML configuration node into an executable build
 *        plan.
 *
 * Accepts an already-loaded YAML::Node representing the user configuration
 * (e.g. the global `config.yaml` shipped with Kez), optionally applies
 * command-line overrides, and transforms it into a BashCommandPlan.  This
 * overload is useful when the configuration has been pre-loaded or generated
 * programmatically rather than read from disk.
 *
 * @param user_config    The user configuration as a YAML::Node.  Typically
 *                       this originates from loading the project's top-level
 *                       `config.yaml` file or a cluster preset.
 * @param install_prefix The root directory under which all packages will be
 *                       installed.
 * @param config_options Optional command-line `-D` overrides forwarded to
 *                       apply_cmdline_config.  Defaults to an empty vector.
 *
 * @return A BashCommandPlan (std::vector<PackageCommands>) containing the
 *         ordered list of packages to build together with their shell command
 *         sequences and inter-package dependencies.
 *
 * @note This overload does not read from disk; the caller is responsible for
 *       loading the YAML node beforehand.
 *
 * @see parse_cmdline(const std::filesystem::path&, ...)
 * @see parse_cmdline(const std::vector<std::string>&, ...)
 * @see write_install_plan
 */
BashCommandPlan parse_cmdline(YAML::Node user_config, const std::filesystem::path& install_prefix,
                              const std::vector<std::string>& config_options = {});

/**
 * @brief Serialise a BashCommandPlan to disk as an install plan script.
 *
 * Writes the given plan to the specified file as a sequence of shell function
 * calls that are understood exclusively by scripts/install.sh.  This is a
 * strict execution boundary between the C++ backend (plan generation) and the
 * bash frontend (plan execution); the output is NOT a package configuration
 * format and should never be edited by hand.
 *
 * The serialised format consists of one shell function invocation per package,
 * encoding its command list and dependency information.  The install.sh script
 * sources and evaluates these functions to drive the actual build and install
 * process.
 *
 * @param plan The BashCommandPlan to serialise.  Must contain at least one
 *             PackageCommands entry; an empty plan produces an empty file.
 * @param path Filesystem path where the install plan script will be written.
 *             If the file already exists it will be overwritten.
 *
 * @warning The output format is an internal implementation detail of the
 *          scripts/install.sh protocol.  Changing either side without updating
 *          the other will break the build pipeline.
 *
 * @see parse_cmdline (all overloads) for how a BashCommandPlan is produced.
 * @see scripts/install.sh for how the serialised plan is consumed.
 */
void write_install_plan(const BashCommandPlan& plan, const std::filesystem::path& path);
