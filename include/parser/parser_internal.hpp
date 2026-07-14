#pragma once

#include <yaml-cpp/yaml.h>

#include <database/database.hpp>
#include <optional>
#include <parser/user_config_parser.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utils/condition_utils.hpp>
#include <vector>

/**
 * @brief Name-to-node lookup tables for one user build configuration.
 *
 * The user configuration stores environment variables and options as YAML
 * sequences.  These maps are built once while loading a package so the
 * fixed-point evaluator can retrieve user values without rescanning those
 * sequences on every pass.
 *
 * Duplicate names retain the first YAML entry, matching the parser's previous
 * linear-search behavior.
 */
struct UserConfigurationIndex {
    /// User environment entries keyed by their @c name field.
    std::unordered_map<std::string, YAML::Node> environment;

    /// User option entries keyed by their @c name field.
    std::unordered_map<std::string, YAML::Node> options;
};

/**
 * @brief A fully-resolved user package combining user configuration with database metadata.
 *
 * During parsing, each entry in the user's configuration is desugared into one of
 * these structs.  It stores the original name the user specified, the raw YAML
 * fragment from the user config, the corresponding database recipe, and an
 * optional transformed build description that may have been modified by
 * template resolution or conditional configuration selection.
 *
 * @note The @p transformed_build is populated lazily; it remains @c std::nullopt
 *       until the build configuration has been resolved against the context's
 *       option and environment state.
 */
struct ParsedUserPackage {
    /// Name of the package as it appears in the user's configuration (may
    /// differ from the database name if aliases or abstract packages are used).
    std::string requested_name;

    /// The raw YAML node from the user configuration that describes this
    /// package's settings (e.g. options, environment overrides).
    YAML::Node user_config;

    /// Shared pointer to the immutable, fully-parsed recipe from the database.
    PackageConfigPtr database_config;

    /// The build description after applying configuration selection and
    /// structural transformations.  @c std::nullopt until the build has been
    /// resolved for this package.
    std::optional<Build> transformed_build;

    /// Indexed values for the top-level @ref Build::configurations entry.
    UserConfigurationIndex build_configuration_index;

    /// Indexed values for build stages, aligned with @ref Build::stages.
    std::vector<UserConfigurationIndex> stage_configuration_indices;
};

/**
 * @brief Tracks the resolved state of a single build option.
 *
 * A build option can be either enabled or disabled.  When enabled the
 * @p enabled_value is used in generated commands; when disabled the
 * @p disabled_value is used instead.  The initial default is @c true
 * (enabled), with empty value strings.
 *
 * @see get_selected_option_value()  Returns the currently active value.
 */
struct ParsedOptionState {
    /// Whether the option is currently enabled.  Defaults to @c true.
    bool enabled = true;

    /// Value substituted into command templates when the option is enabled.
    std::string enabled_value;

    /// Value substituted into command templates when the option is disabled.
    std::string disabled_value;
};

/**
 * @brief Return the currently applicable value for an option based on its state.
 *
 * @param state  The parsed option state to query.
 * @return A const reference to @p state.enabled_value if the option is enabled,
 *         or @p state.disabled_value if it is disabled.
 */
inline const std::string& get_selected_option_value(const ParsedOptionState& state) {
    return state.enabled ? state.enabled_value : state.disabled_value;
}

/**
 * @brief Mutable context threaded through the entire user-configuration parsing pipeline.
 *
 * This struct aggregates the user's raw configuration, the parser settings,
 * every partially- or fully-resolved package, and all index/alias/lookup tables
 * that must be shared across parsing phases.  A single instance is created at
 * the top of the parse and passed by non-const reference to every internal
 * helper.
 *
 * ## Phase ordering
 *
 * 1. The user's package list is read and each entry is desugared into a
 *    @ref ParsedUserPackage, indexed by name in @p package_indices.
 * 2. Aliases (@p package_aliases) and abstract-package selections
 *    (@p abstract_packages) are resolved.
 * 3. Extra configurations (@p extra_configs) are loaded for external/system
 *    packages.
 * 4. Options and environment variables are resolved against the database
 *    metadata, storing results in @p option_values, @p environment_values,
 *    @p named_option_values, and @p named_environment_values.
 * 5. Template resolution and condition evaluation use @p resolving_templates
 *    to detect recursive references.
 *
 * @warning All maps and sets are modified in place.  The caller must ensure
 *          that no two phases run concurrently.
 */
struct UserConfigParserContext {
    /// The top-level user configuration YAML node (the full document).
    YAML::Node user_config;

    /// Global parser settings (paths, architecture, external packages, etc.).
    UserConfigParserSettings settings;

    /// Ordered list of all parsed user packages, in the order they appear
    /// in the user configuration (de-sugared from the "packages" section).
    std::vector<ParsedUserPackage> packages;

    /// Maps a package name (as it appears in the user config or after alias
    /// resolution) to its index in @p packages.
    std::unordered_map<std::string, std::size_t> package_indices;

    /// Maps user-specified aliases to their canonical package names.
    std::unordered_map<std::string, std::string> package_aliases;

    /// Extra database configurations for packages that are not part of the
    /// user's explicit install list (e.g. system-implicit or external packages).
    /// Keyed by package name.
    std::unordered_map<std::string, PackageConfigPtr> extra_configs;

    /// Maps an abstract-package name to the concrete implementation chosen
    /// by the user or by heuristics.
    std::unordered_map<std::string, std::string> abstract_packages;

    /// Set of all packages that appear as dependencies (direct or transitive)
    /// across the entire install plan.
    std::unordered_set<std::string> dependencies;

    /// Resolved value for each environment variable defined in the database
    /// recipes, keyed by pointer identity of the @ref EnvironmentVariable.
    std::unordered_map<const EnvironmentVariable*, std::string> environment_values;

    /// Resolved enabled/disabled state for each build option defined in the
    /// database recipes, keyed by pointer identity of the @ref BuildOption.
    std::unordered_map<const BuildOption*, ParsedOptionState> option_values;

    /// Resolved environment variable values keyed by a user-assigned name
    /// (from the user config's "environment" section).
    std::unordered_map<std::string, std::string> named_environment_values;

    /// Resolved option states keyed by a user-assigned name
    /// (from the user config's "options" section).
    std::unordered_map<std::string, ParsedOptionState> named_option_values;

    /// Cache of parsed condition expressions, keyed by the raw expression
    /// string.  Filled on demand by evaluate_parser_condition() so that the
    /// fixed-point convergence loop does not re-tokenise and re-parse the
    /// same conditions on every pass.
    std::unordered_map<std::string, ConditionExpr> condition_parse_cache;

    /// Monotonically increasing version counter bumped on every option or
    /// environment-value change.  Used by precompute_parser_values() to detect
    /// convergence in O(1) instead of comparing the full maps.
    std::size_t config_version = 0;

    /// Set of template names currently being resolved, used to detect and
    /// reject recursive template expansions.
    std::unordered_set<std::string> resolving_templates;

    /// Name of the package currently being processed.  Used for error
    /// messages and to scope template resolution.
    std::string current_package;
};

/**
 * @brief Print an error message and terminate the program.
 *
 * Reports a fatal error originating from user-configuration parsing.  The
 * message is printed via the ERROR macro (coloured output) and the process
 * exits with a non-zero status.
 *
 * @param message  Descriptive error text explaining what went wrong.
 *
 * @warning This function never returns (marked @c [[noreturn]]).
 */
[[noreturn]] void user_config_error(const std::string& message);

/**
 * @brief Return database metadata for a package referenced during parsing.
 *
 * Resolves aliases, consults the parsed-package and extra-configuration
 * caches, and loads the package from the database only when necessary.
 *
 * @param context       Mutable parser context containing package caches.
 * @param package_name  Package name or canonical database alias.
 * @return Shared immutable package configuration.
 */
PackageConfigPtr parser_package_config(UserConfigParserContext& context,
                                       const std::string& package_name);

/**
 * @brief Evaluate one user-configuration condition expression.
 *
 * @param expression  Condition text using the parser condition grammar.
 * @param context     Current option, environment, package, and version state.
 * @return true when the expression evaluates successfully to true.
 */
bool evaluate_parser_condition(const std::string& expression, UserConfigParserContext& context);

/**
 * @brief Evaluate conditional overrides for a configurable string value and
 *        return the resulting value.
 *
 * Walks the list of @ref ConditionalValue entries attached to a
 * ConfigurableValue@<std::string@>.  For each entry whose condition
 * evaluates to true in the current @p context, the corresponding action
 * (Set / Append / Prepend) is applied to @p base_value.  The first matching
 * entry with action Set replaces @p base_value entirely; Append and Prepend
 * modify it in place.  If no condition matches, @p base_value is returned
 * unchanged.
 *
 * @param configurable  The configurable value whose conditions are evaluated.
 * @param base_value    The starting value before any conditional overrides.
 * @param context       The parser context used to evaluate conditions.
 * @return The final string after applying all matching conditions.
 */
std::string apply_parser_conditions(const ConfigurableValue<std::string>& configurable,
                                    const std::string& base_value,
                                    UserConfigParserContext& context);

/**
 * @brief Evaluate conditional overrides for a configurable boolean value.
 *
 * Similar to the string overload, but operates on a
 * ConfigurableValue@<bool@>.  Each matching condition's action is
 * applied to the boolean base value.  Note that Append and Prepend behave
 * identically to Set for boolean values (the boolean is simply replaced).
 *
 * @param configurable  The configurable value whose conditions are evaluated.
 * @param base_value    The starting boolean before any conditional overrides.
 * @param context       The parser context used to evaluate conditions.
 * @return The final boolean after applying all matching conditions.
 */
bool apply_parser_conditions(const ConfigurableValue<bool>& configurable, bool base_value,
                             UserConfigParserContext& context);

/**
 * @brief Resolve option and environment values to a fixed point.
 *
 * Performs an unconditional initialization pass followed by conditional
 * passes until no indexed value changes.
 *
 * @param context Parser context to update in place.
 */
void precompute_parser_values(UserConfigParserContext& context);

/**
 * @brief Resolve template placeholders and variable references in a scalar
 *        string value.
 *
 * Scans @p value for template markers (e.g. @c ${prefix}, @c ${package_name},
 * references to environment variables or build options) and substitutes the
 * resolved text from @p context.  Recursive references are detected via
 * @p context.resolving_templates.
 *
 * @param value    The scalar string that may contain template placeholders.
 * @param context  The parser context providing variable values and tracking
 *                 recursive resolution.
 * @return The fully-resolved string with all placeholders expanded.
 *
 * @see parser_package_prefix()  For resolving @c ${prefix} references.
 */
std::string resolve_parser_scalar(const std::string& value, UserConfigParserContext& context);

/**
 * @brief Resolve the installation prefix for a given package.
 *
 * Returns the absolute path under which the package's build artifacts are
 * installed.  The prefix is typically derived from the architecture and the
 * package name (e.g. @c &lt;install_prefix&gt;/&lt;arch&gt;/&lt;pkg&gt;).
 *
 * @param package_name  The canonical name of the package.
 * @param context       The parser context containing installation-prefix
 *                      settings.
 * @return The absolute installation prefix path as a string.
 *
 * @see resolve_parser_scalar()  For how @c ${prefix} is expanded inside templates.
 */
std::string parser_package_prefix(const std::string& package_name,
                                  UserConfigParserContext& context);

/**
 * @brief Generate the shell source commands for a resolved user package and
 *        append them to the command list.
 *
 * Produces the sequence of bash commands needed to download, configure, build,
 * and install the package described by @p package.  The commands are generated
 * by resolving the package's source, build stages, options, environment
 * variables, and postprocessing steps against the current @p context, then
 * appended to @p commands in execution order.
 *
 * @param package   The fully-resolved user package describing what to build.
 * @param context   The parser context providing settings, option states, and
 *                  environment variable values.
 * @param commands  Output vector to which the generated source commands are
 *                  appended.  Existing entries are preserved.
 *
 * @note Dependencies declared by the package are expected to be processed
 *       separately (typically by the dependency resolver); this function
 *       generates only the commands directly belonging to @p package.
 */
void append_source_commands(const ParsedUserPackage& package, UserConfigParserContext& context,
                            std::vector<std::string>& commands);

/**
 * @brief Render all shell commands for one parsed package.
 *
 * @param package Parsed package and transformed build description.
 * @param context Parser state used for templates and resolved values.
 * @return Ordered shell commands for the package.
 */
std::vector<std::string> generate_package_commands(const ParsedUserPackage& package,
                                                   UserConfigParserContext& context);

/**
 * @brief Generate executable-plan dependency edges for one package.
 *
 * Traverses direct package dependencies and configuration requirements,
 * collapsing non-buildable facade packages onto buildable descendants.
 *
 * @param package             Package whose edges are generated.
 * @param context             Parsed package graph and abstract selections.
 * @param buildable_packages  Packages that emit executable commands.
 * @return Ordered, duplicate-free buildable dependency names.
 */
std::vector<std::string> generate_package_dependencies(
    const ParsedUserPackage& package, const UserConfigParserContext& context,
    const std::unordered_set<std::string>& buildable_packages);
