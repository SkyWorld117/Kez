#pragma once

#include <algorithm>
#include <optional>
#include <string>
#include <variant>
#include <vector>

/**
 * @brief Categorizes the nature of a package in the Kez database.
 *
 * The type controls how the system interprets and processes a package
 * recipe.  For example, @c Abstract packages serve as pure interfaces
 * that must be implemented by a concrete package, while @c System
 * packages are assumed to be provided by the operating system and are
 * never built from source.
 */
enum class PackageType {
    Package,   ///< A regular, downloadable package built from source.
    System,    ///< Provided by the system package manager; not built.
    Compiler,  ///< A compiler toolchain (e.g. gcc, llvm).
    Mpi,       ///< An MPI implementation (e.g. openmpi, mpich).
    Vendor,    ///< A vendor-provided blob or SDK (e.g. CUDA, MKL).
    Abstract,  ///< A virtual / interface-only package; requires an
               ///< implementation listed in @ref
               ///< PackageConfig::implementations.
    External,  ///< Managed externally outside the Kez environment.
};

/**
 * @brief Identifies the build-system toolchain a package uses.
 *
 * Each value corresponds to a concrete subclass of @ref PackageConfig
 * that knows how to produce sensible default configure and build
 * commands for that toolchain.
 */
enum class Toolchain {
    None,       ///< No specific toolchain; use @ref GenericPackageConfig.
    Autotools,  ///< GNU Autotools (autoconf / automake / libtool).
    CMake,      ///< CMake-based build system.
    Make,       ///< Plain Makefiles.
};

/**
 * @brief Describes the kind of source artifact a package fetches.
 */
enum class SourceType {
    Git,      ///< Git repository clone (uses a tag or branch).
    Tarball,  ///< Compressed tarball archive (`.tar.gz`, `.tar.bz2`, etc.).
    Zip,      ///< ZIP archive.
    Script,   ///< An inline script fragment that produces the source tree.
};

/**
 * @brief Specifies how a conditional value is combined with an existing or
 *        default value.
 *
 * This is used by @ref ConditionalValue to control whether the matched
 * value replaces, augments, or precedes the base setting.
 */
enum class ValueAction {
    Set,      ///< Replace any existing value entirely.
    Append,   ///< Add the value after the existing value (e.g. to a list).
    Prepend,  ///< Add the value before the existing value.
};

/**
 * @brief A value that is applied only when a string-based condition
 *        expression evaluates to true at resolution time.
 *
 * Condition expressions reference properties of the package and its
 * dependencies (e.g. ``"lib"``, ``"incflags"``) and are evaluated by
 * the condition parser.  The @ref action field controls how @ref value
 * is combined with the base (default) value.
 *
 * @tparam T The value type — typically @c std::string or @c bool.
 */
template <typename T> struct ConditionalValue {
    /** @brief Condition expression string (evaluated at resolution time). */
    std::string condition;
    /** @brief How to combine this value with the default (default: @c Set). */
    ValueAction action = ValueAction::Set;
    /** @brief The value to apply when the condition is true. */
    T value;
};

/**
 * @brief A value that carries an optional default plus a list of
 *        condition-dependent overrides.
 *
 * During user-config resolution the conditions are evaluated in order;
 * the first matching condition's value is used.  If no condition matches
 * and a @ref default_value is present, that fallback is used.
 *
 * @tparam T The value type — typically @c std::string or @c bool.
 */
template <typename T> struct ConfigurableValue {
    /** @brief Fallback value used when no condition matches (may be
     *         disengaged, meaning an empty / no value). */
    std::optional<T> default_value;
    /** @brief Ordered list of conditional overrides; evaluated
     *         first-match wins. */
    std::vector<ConditionalValue<T>> conditions;
};

/**
 * @brief Identifies a specific release (version) of a source package.
 *
 * A @ref Source can list multiple @c Release entries so that the same
 * recipe can track several versions (e.g. stable and development).
 */
struct Release {
    /** @brief Version string (e.g. ``"3.1.4"``). */
    std::string version;
    /** @brief Download URL for this release.
     *
     * Required when @ref SourceType is @c Tarball or @c Zip.
     * For @c Git sources the repository-level @ref Source::url is used
     * instead, and only @ref tag is relevant per release. */
    std::optional<std::string> url;
    /** @brief Git tag or branch name to check out (required when
     *         @ref SourceType is @c Git). */
    std::optional<std::string> tag;
};

/**
 * @brief Describes where and how to obtain the package source code.
 *
 * Depending on @ref type, different fields are relevant:
 * - For @c Git: @ref url (the repository URL) and each @ref Release::tag
 *   (the commit/branch to check out) are required.
 * - For @c Tarball / @c Zip: each @ref Release must supply its own
 *   download URL via @ref Release::url; @ref url at the source level
 *   is not used for downloading.
 * - For @c Script: the source is generated by an inline recipe fragment;
 *   URLs are typically not needed.
 */
struct Source {
    /** @brief The kind of source artifact. */
    SourceType type;
    /** @brief Default download URL; may contain placeholders resolved
     *         per-release. */
    std::optional<std::string> url;
    /** @brief Available releases.  The first entry is considered the
     *         default version. */
    std::vector<Release> releases;
};

/**
 * @brief An environment variable injected during a build stage.
 *
 * Variables can be marked as user-configurable so that an end-user may
 * override their value from the top-level @c config.yaml.  The
 * @ref requires list declares dependencies whose properties must be
 * resolvable before the variable can be evaluated.
 */
struct EnvironmentVariable {
    /** @brief Variable name (e.g. ``CC``, ``CFLAGS``). */
    std::string name;
    /** @brief Human-readable description of the variable's purpose. */
    std::optional<std::string> description;
    /** @brief Whether end-users can override this variable from their
     *         configuration (default: @c false). */
    bool user_configurable = false;
    /** @brief Packages whose properties must be resolvable before this
     *         variable can be evaluated. */
    std::vector<std::string>
        requires;
    /** @brief The variable's default and any condition-dependent
     *         overrides. */
    ConfigurableValue<std::string> value;
};

/**
 * @brief A configurable build-system flag or toggle (e.g. a CMake
 *        ``-DBUILD_SHARED_LIBS=ON``).
 *
 * Options can expose an enabled/disabled boolean toggle, an arbitrary
 * string value per state, or both.  The @ref enabled_format /
 * @ref disabled_format fields specify the literal flag name to emit
 * (optionally prefixed by the toolchain, e.g. ``-D`` for CMake).
 * The resolved value is appended with ``=`` when non-empty.
 */
struct BuildOption {
    /** @brief Short name of the option (e.g. ``shared_libs``). */
    std::string name;
    /** @brief Human-readable description of what the option controls. */
    std::optional<std::string> description;
    /** @brief Whether end-users can override this option from their
     *         configuration (default: @c false). */
    bool user_configurable = false;
    /** @brief Condition-dependent boolean enabling/disabling the option
     *         (disengaged means always enabled). */
    std::optional<ConfigurableValue<bool>> enabled;
    /** @brief When the option is enabled, use this string as the flag name
     *         instead of @ref name (e.g. ``"FEATURE=ON"``); if absent,
     *         @ref name is used as the flag. */
    std::optional<std::string> enabled_format;
    /** @brief When the option is disabled, use this string as the flag name
     *         instead of @ref name (e.g. ``"FEATURE=OFF"``); if absent,
     *         no flag is emitted for the disabled state. */
    std::optional<std::string> disabled_format;
    /** @brief Packages that must be present for this option to be valid. */
    std::vector<std::string>
        requires;
    /** @brief Condition-dependent string value used when the option is
     *         enabled (e.g. ``"ON"`` or ``"shared"``). */
    std::optional<ConfigurableValue<std::string>> enabled_value;
    /** @brief Condition-dependent string value used when the option is
     *         disabled (e.g. ``"OFF"`` or ``"static"``). */
    std::optional<ConfigurableValue<std::string>> disabled_value;
};

/**
 * @brief Shared configuration applied to one or more build stages.
 *
 * Contains the command template, a set of environment variables, and a
 * list of build options.  A @ref BuildConfiguration can appear at the
 * top-level of a @ref Build (applying to all stages) or be overridden
 * per @ref BuildStage.
 */
struct BuildConfiguration {
    /** @brief Shell command for the configure step (e.g.
     *         ``"./configure"`` or ``"cmake -B build"``).  If absent, a
     *         toolchain-specific default is used; options and environment
     *         variables are appended as separate flags. */
    std::optional<std::string> command;
    /** @brief Environment variables to set before running the command. */
    std::vector<EnvironmentVariable> environment;
    /** @brief Build options (flags, toggles) to pass. */
    std::vector<BuildOption> options;
};

/**
 * @brief One stage in a multi-step build process.
 *
 * Typical stages are "configure", "build", and "install".  Each stage
 * can declare its own @ref configurations that override or supplement
 * the top-level build configuration.
 */
struct BuildStage {
    /** @brief Build target name (e.g. ``"all"``, ``"install"``) or
     *         @c std::nullopt to use the toolchain default. */
    std::optional<std::string> target;
    /** @brief Whether parallel (multi-threaded) execution is safe for
     *         this stage (default: @c true). */
    bool multithreaded = true;
    /** @brief Per-stage configuration (environment variables and build
     *         options) applied independently alongside the top-level
     *         @ref Build::configurations when this stage is executed. */
    std::optional<BuildConfiguration> configurations;
};

/**
 * @brief Complete build description for a package.
 *
 * Consists of optional pre- and post-processing shell fragments, a
 * top-level @ref BuildConfiguration, and an ordered list of
 * @ref BuildStage entries.
 */
struct Build {
    /** @brief Shell commands run before any build stage (e.g. patching
     *         the source tree). */
    std::optional<std::string> preprocessing;
    /** @brief Shell commands run after all build stages succeed (e.g.
     *         stripping binaries or installing extras). */
    std::optional<std::string> postprocessing;
    /** @brief Top-level configuration applied to every build stage
     *         unless overridden by a stage-local one. */
    std::optional<BuildConfiguration> configurations;
    /** @brief Ordered list of build stages.  Each stage is processed in
     *         order; toolchain-specific default commands are selected per
     *         stage when the stage does not supply an explicit command. */
    std::vector<BuildStage> stages;
};

/**
 * @brief An override that modifies a property of a dependency at
 *        resolution time.
 *
 * Overrides are evaluated when the user configuration is being resolved
 * and can alter the values a dependency exposes (e.g. force a
 * different include path or library name).  The optional @ref condition
 * makes the override apply only when a given predicate holds.
 */
struct Override {
    /**
     * @brief Optional condition expression.  When present, the override
     *        is applied only if the expression evaluates to true;
     *        when absent the override always applies.
     */
    std::optional<std::string> condition;
    /** @brief Template variable expression identifying the property to
     *         override (e.g. ``"${library.libs}"``).  The expression is
     *         matched against template references during resolution. */
    std::string target;
    /** @brief How the override value is combined with the existing one
     *         (default: @c Set). */
    ValueAction action = ValueAction::Set;
    /** @brief The new value to apply. */
    std::string value;
};

/**
 * @brief Holds the resolved data for a named package property.
 *
 * Property data can be either a plain @c std::string or a
 * @ref ConfigurableValue<std::string> with condition-dependent
 * overrides.  Typical property names are ``"include"``, ``"lib"``,
 * ``"cflags"``, ``"ldflags"``, ``"bin"``, etc.
 */
using PropertyData = std::variant<std::string, ConfigurableValue<std::string>>;

/**
 * @brief A named package property with associated data.
 *
 * Properties are the mechanism by which packages declare how they
 * should be consumed by dependents (e.g. include directories, link
 * libraries, compiler flags).
 *
 * @see find_property
 * @see has_property
 */
struct Property {
    /** @brief Property name (e.g. ``"include"``, ``"lib"``). */
    std::string name;
    /** @brief Property value, either a plain string or a
     *         condition-dependent configurable. */
    PropertyData data;
};

/**
 * @brief Base class representing a single parsed package recipe.
 *
 * @c PackageConfig holds all metadata parsed from a package's
 * ``latest.yaml``: identity fields (@ref name, @ref description,
 * @ref author, @ref type), source information, dependency list,
 * overrides, build description, properties, and implementation
 * references.
 *
 * Subclasses override @ref toolchain(), @ref default_configuration_command(),
 * and @ref default_stage_command() to provide toolchain-specific
 * defaults.  Use @c std::shared_ptr<const PackageConfig> (aliased as
 * @ref PackageConfigPtr) to manage ownership.
 *
 * @see GenericPackageConfig
 * @see AutotoolsPackageConfig
 * @see CMakePackageConfig
 * @see MakePackageConfig
 */
class PackageConfig {
   public:
    virtual ~PackageConfig() = default;

    /**
     * @brief Returns the build-system toolchain identifier for this package.
     * @return A @ref Toolchain enumerator indicating which build system
     *         the recipe uses.
     */
    virtual Toolchain toolchain() const noexcept = 0;

    /**
     * @brief Returns a toolchain-appropriate default configuration
     *        command when the recipe does not supply one.
     *
     * The base implementation returns @c std::nullopt (no default).
     *
     * @return A command string, or @c std::nullopt if no sensible
     *         default exists for this toolchain.
     */
    virtual std::optional<std::string> default_configuration_command() const;

    /**
     * @brief Returns a toolchain-appropriate default stage command
     *        when the recipe does not supply one for the given stage.
     *
     * @param stage         The build stage for which a command is needed.
     * @param parallel_jobs Number of parallel jobs to use (e.g.
     *                      ``make -j{N}``).
     * @return A command string, or @c std::nullopt if no sensible
     *         default exists.
     */
    virtual std::optional<std::string> default_stage_command(const BuildStage& stage,
                                                             unsigned int parallel_jobs) const;

    // ---- Parsed recipe fields -------------------------------------------

    /** @brief Package name (unique identifier, e.g. ``"zlib"``). */
    std::string name;
    /** @brief Optional human-readable description of the package. */
    std::optional<std::string> description;
    /** @brief Optional author or maintainer of the recipe. */
    std::optional<std::string> author;
    /** @brief Package type (default: @c PackageType::Package). */
    PackageType type = PackageType::Package;
    /** @brief Source code location and versioning information. */
    std::optional<Source> source;
    /** @brief List of package names this package directly depends on. */
    std::vector<std::string> dependencies;
    /** @brief Property overrides applied to dependencies at resolution
     *         time. */
    std::vector<Override> overrides;
    /** @brief Build description (pre/post processing, stages,
     *         configurations, options). */
    std::optional<Build> build;
    /** @brief Named properties advertised to dependents (include dirs,
     *         libs, flags, etc.). */
    std::vector<Property> properties;
    /** @brief Concrete package names that implement this package.
     *         Required when @ref type is @c Abstract; otherwise
     *         typically empty. */
    std::vector<std::string> implementations;
};

/**
 * @brief Concrete package configuration for recipes that do not specify
 *        a toolchain (i.e. ``toolchain`` is absent from the recipe).
 *
 * @c GenericPackageConfig returns @ref Toolchain::None and provides no
 * default configuration or stage commands.  The recipe must supply all
 * build commands explicitly through the @ref Build structure.
 */
class GenericPackageConfig final : public PackageConfig {
   public:
    Toolchain toolchain() const noexcept override;
};

/**
 * @brief Concrete package configuration for GNU Autotools-based
 *        packages (``toolchain: autotools``).
 *
 * Provides sensible defaults:
 * - Configuration command: ``./configure``
 *   (the install prefix and other flags are appended as separate
 *   options by @ref user_config_generator::transformed_configuration).
 * - Build stage: ``make -j{N}``
 * - Install stage: ``make install``
 */
class AutotoolsPackageConfig final : public PackageConfig {
   public:
    Toolchain toolchain() const noexcept override;
    std::optional<std::string> default_configuration_command() const override;
    std::optional<std::string> default_stage_command(const BuildStage& stage,
                                                     unsigned int parallel_jobs) const override;
};

/**
 * @brief Concrete package configuration for CMake-based packages
 *        (``toolchain: cmake``).
 *
 * Provides sensible defaults:
 * - Configuration command: ``cmake -B build``
 *   (additional CMake flags such as ``-DCMAKE_INSTALL_PREFIX``
 *   are appended as separate options by
 *   @ref user_config_generator::transformed_configuration).
 * - Build stage: ``cmake --build build --parallel {N}``
 * - Install stage: ``cmake --install build``
 */
class CMakePackageConfig final : public PackageConfig {
   public:
    Toolchain toolchain() const noexcept override;
    std::optional<std::string> default_configuration_command() const override;
    std::optional<std::string> default_stage_command(const BuildStage& stage,
                                                     unsigned int parallel_jobs) const override;
};

/**
 * @brief Concrete package configuration for plain Makefile-based
 *        packages (``toolchain: make``).
 *
 * Provides sensible defaults:
 * - Build stage: ``make -j{N} {target}``
 * - Install stage: ``make install``
 *
 * No default configuration command is provided because Makefiles
 * typically do not have a separate configure step.
 */
class MakePackageConfig final : public PackageConfig {
   public:
    Toolchain toolchain() const noexcept override;
    std::optional<std::string> default_stage_command(const BuildStage& stage,
                                                     unsigned int parallel_jobs) const override;
};

/**
 * @brief Locate a property by name within a package configuration.
 *
 * Performs a linear scan over @p config.properties and returns a
 * pointer to the matching @ref Property, or @c nullptr if no property
 * with the given name exists.
 *
 * @param config The package configuration to search.
 * @param name   The property name to look for.
 * @return Pointer to the matching property, or @c nullptr if not found.
 *
 * @see has_property
 */
inline const Property* find_property(const PackageConfig& config, const std::string& name) {
    const auto property =
        std::find_if(config.properties.begin(), config.properties.end(),
                     [&name](const Property& candidate) { return candidate.name == name; });
    return property == config.properties.end() ? nullptr : &*property;
}

/**
 * @brief Check whether a package configuration declares a given property.
 *
 * This function recognises several property-name aliases for
 * convenience:
 * | Requested name  | Also checks for |
 * |-----------------|-----------------|
 * | ``"incflags"``  | ``"include"``   |
 * | ``"ldflags"``   | ``"lib"``       |
 * | ``"nvldflags"`` | ``"lib"``       |
 *
 * @param config   The package configuration to inspect.
 * @param property The property name (or alias) to query.
 * @return @c true if the property (or its alias) exists, @c false
 *         otherwise.
 *
 * @see find_property
 */
inline bool has_property(const PackageConfig& config, const std::string& property) {
    if (find_property(config, property) != nullptr) {
        return true;
    }
    if (property == "incflags") {
        return find_property(config, "include") != nullptr;
    }
    if (property == "ldflags" || property == "nvldflags") {
        return find_property(config, "lib") != nullptr;
    }
    return false;
}
