#pragma once

#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief Settings that describe an external (pre-installed) software package.
 *
 * When a package is not built by Kez but is expected to already exist on the
 * system (e.g. system-level MPI or BLAS installations), this struct captures
 * its location and version so the generated install commands can reference
 * them without recompiling the package.
 */
struct ExternalPackageSettings {
    /** @brief Absolute path to the root of the external installation. */
    std::string prefix;

    /** @brief Version string identifying the external installation. */
    std::string version;
};

/**
 * @brief Aggregated configuration used throughout the user-config parsing
 *        pipeline.
 *
 * Populated either by reading a `config.yaml` file from disk via
 * load_user_config_parser_settings() or by constructing directly in tests.
 * Every path member is expected to be an absolute, existing directory.
 */
struct UserConfigParserSettings {
    /**
     * @brief Root directory where the target environment's packages are
     *        installed (e.g. ``<KEZ_WORKDIR>/applications/<env-name>``
     *        for application environments).
     */
    std::filesystem::path install_prefix;

    /**
     * @brief Resolve a managed top-level target from @ref install_prefix.
     *
     * Enabled by ``kez install --rename`` so compiler, MPI, and vendor prefix
     * templates use the renamed root while their package versions remain
     * unchanged.
     */
    bool use_install_prefix_for_managed_target = false;

    /**
     * @brief Root of the Kez project tree (points to the directory
     *        containing main.sh, the Makefile, etc.).
     */
    std::filesystem::path kez_home;

    /**
     * @brief System-level prefix, typically /usr or /usr/local.
     *
     * Used as a fallback when searching for system-provided dependencies.
     */
    std::filesystem::path system_prefix;

    /**
     * @brief Directory where compiler installations reside
     *        (KEZ_WORKDIR/compilers by convention).
     */
    std::filesystem::path compilers_prefix;

    /**
     * @brief Directory where MPI installations reside
     *        (KEZ_WORKDIR/mpis by convention).
     */
    std::filesystem::path mpis_prefix;

    /**
     * @brief Directory where vendor-provided library installations reside
     *        (KEZ_WORKDIR/vendors by convention).
     */
    std::filesystem::path vendors_prefix;

    /**
     * @brief Directory used for caching downloaded sources and intermediate
     *        build artifacts.
     */
    std::filesystem::path cache_prefix;

    /**
     * @brief Number of parallel build jobs to use when compiling packages.
     *
     * Defaults to 1. Translated to the -j flag passed to the build system
     * (e.g. make -j<N>).  Controlled by the @c KEZ_NPROC environment
     * variable and the @c settings.n_proc_for_build config key.
     */
    unsigned int parallel_jobs = 1;

    /**
     * @brief Target CPU micro-architecture, e.g. "x86_64", "aarch64".
     *
     * Controls which architecture-specific heuristics and compiler flags
     * are selected during plan generation.
     */
    std::string architecture;

    /**
     * @brief Map of architecture variant names to their concrete values.
     *
     * Variants refine the base architecture, e.g.
     * { "cpu": "skylake-avx512", "fpu": "avx512f" }.
     * Consulted by the advisor when selecting optimal library substitutes.
     */
    std::unordered_map<std::string, std::string> architecture_variants;

    /**
     * @brief Map of external (pre-installed) packages that should be used
     *        instead of building them from source.
     *
     * Keyed by the abstract package name; value holds the installation
     * prefix and version. Populated from the user's config.yaml.
     *
     * @see ExternalPackageSettings
     */
    std::unordered_map<std::string, ExternalPackageSettings> external_packages;
};

/**
 * @brief A single package's install plan: the list of shell commands to
 *        execute and the packages it depends on.
 *
 * Produced by parsing a resolved user configuration file. The commands
 * are later consumed by scripts/install.sh, which executes them in an
 * order respecting the inter-package dependency graph.
 */
struct PackageCommands {
    /** @brief Name of the package these commands belong to. */
    std::string package;

    /**
     * @brief Shell commands to execute in order for this package.
     *
     * Each string is a single command or a compound shell expression
     * (e.g. "make -j4 && make install"). Commands are run by
     * scripts/install.sh with proper error handling.
     */
    std::vector<std::string> commands;

    /**
     * @brief Names of packages that must be installed before this one.
     *
     * Used to order the plan topologically and to produce the
     * @c dependencies entry in the generated user config.
     */
    std::vector<std::string> dependencies;
};

/**
 * @brief The full installation plan as an ordered sequence of per-package
 *        command groups.
 *
 * The order of elements respects the dependency graph: when package A
 * depends on package B, A appears after B in the vector.  The plan is
 * produced by parse_user_config() / parse_user_config_file() and consumed
 * by the install pipeline.
 *
 * @see PackageCommands
 * @see parse_user_config
 * @see parse_user_config_file
 */
using BashCommandPlan = std::vector<PackageCommands>;

/**
 * @brief Load parser settings from environment variables and configuration
 *        files.
 *
 * Reads the YAML manifest (``<KEZ_HOME>/manifest.yaml``) and the user's
 * global config (``<KEZ_WORKDIR>/config.yaml``) to populate a
 * UserConfigParserSettings struct with install prefixes, architecture
 * settings, parallelism level, and external-package definitions.
 *
 * The following environment variables are required:
 *   - ``KEZ_HOME``  -- Root of the Kez project tree.
 *   - ``KEZ_WORKDIR`` -- Working directory with config.yaml and state.
 *   - ``KEZ_ARCH``   -- Target architecture (e.g. ``"x86_64"``).
 *
 * @param install_prefix  Root directory of the target environment under
 *                        which packages will be installed.  This value
 *                        is stored in the returned settings but does
 *                        **not** affect which files are read.
 *
 * @return A fully populated UserConfigParserSettings struct.
 *
 * @warning Terminates the process if any required environment variable is
 *          unset, or if the manifest or config file is missing or invalid.
 *
 * @see UserConfigParserSettings
 * @see parse_user_config
 */
UserConfigParserSettings load_user_config_parser_settings(
    const std::filesystem::path& install_prefix);

/**
 * @brief Parse a pre-loaded YAML node into an executable BashCommandPlan.
 *
 * Interprets the configuration produced by the uconf_generator
 * pipeline.  The @p settings provide filesystem paths, architecture
 * details, external-package overrides, and the desired parallelism level.
 * Template markers in the YAML are resolved through the template resolver.
 *
 * @param user_config  The YAML::Node representing the parsed user
 *                     configuration (typically the top-level node of
 *                     a user's config file).
 * @param settings     Parsing settings controlling paths, architecture,
 *                     and external package configuration.
 *
 * @return A BashCommandPlan with one element per package, ordered
 *         topologically by dependencies.
 *
 * @see UserConfigParserSettings
 * @see BashCommandPlan
 * @see parse_user_config_file
 */
BashCommandPlan parse_user_config(const YAML::Node& user_config,
                                  const UserConfigParserSettings& settings);

/**
 * @brief Parse a pre-loaded YAML node into a BashCommandPlan, inferring
 *        settings from a given install prefix.
 *
 * Convenience overload that calls load_user_config_parser_settings()
 * internally.  Equivalent to:
 * @code
 *   auto s = load_user_config_parser_settings(install_prefix);
 *   return parse_user_config(user_config, s);
 * @endcode
 *
 * @param user_config    The YAML::Node to parse.
 * @param install_prefix Root directory used to load the configuration
 *                       settings via load_user_config_parser_settings().
 *
 * @return A BashCommandPlan ordered topologically by dependencies.
 *
 * @see parse_user_config(const YAML::Node&, const UserConfigParserSettings&)
 * @see load_user_config_parser_settings
 */
BashCommandPlan parse_user_config(const YAML::Node& user_config,
                                  const std::filesystem::path& install_prefix);

/**
 * @brief Parse a user configuration file from disk into a
 *        BashCommandPlan.
 *
 * Opens and parses the YAML file at @p path, then delegates to
 * parse_user_config() with the given @p settings.  Terminates the
 * process with an error message if the file cannot be opened or does
 * not contain valid YAML.
 *
 * @param path     Filesystem path to the user configuration YAML file.
 * @param settings Parsing settings controlling paths, architecture,
 *                 and external package configuration.
 *
 * @return A BashCommandPlan ordered topologically by dependencies.
 *
 * @see parse_user_config(const YAML::Node&, const UserConfigParserSettings&)
 * @see UserConfigParserSettings
 * @see BashCommandPlan
 */
BashCommandPlan parse_user_config_file(const std::filesystem::path& path,
                                       const UserConfigParserSettings& settings);
