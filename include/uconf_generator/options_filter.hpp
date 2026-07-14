#pragma once

#include <yaml-cpp/yaml.h>

#include <database/config.hpp>
#include <dependency_resolver/resolve_dependencies.hpp>
#include <string>
#include <vector>

/**
 * @brief Filter build options based on user-configurability and dependency satisfaction.
 *
 * Iterates over the given list of @p options and retains only those that are
 * marked as user-configurable AND whose declared requirements are satisfied by
 * the resolved dependency set (see `requirements_satisfied()`).
 *
 * For each retained option the function produces a YAML map node containing the
 * option's name, its description (if any), its default enabled state, and its
 * default enabled/disabled values.  Options whose requirements are not met are
 * silently omitted from the output sequence.
 *
 * The resulting YAML sequence is consumed downstream by the user config
 * generator pipeline (e.g. `config_transformer` and `uconf_generator`)
 * to produce the final user-editable configuration file.
 *
 * @param options            The raw build options from a package recipe's
 *                           `BuildConfiguration`.  Each `BuildOption` carries
 *                           a name, optional description, user-configurability
 *                           flag, optional default enable states, and optional
 *                           requirement expressions.
 * @param all_dependencies   The full list of concrete package names that will
 *                           be installed.  Used to evaluate each option's
 *                           `requires` field via linear scan.  This is the
 *                           union of essential and optional dependencies
 *                           produced by the dependency resolver.
 * @param abstract_packages  Mapping from abstract package names (e.g. "BLAS",
 *                           "LAPACK") to concrete package selections (e.g.
 *                           "openblas", "netlib-lapack").  When an option's
 *                           `requires` entry is an abstract key, it is
 *                           resolved through this map before checking
 *                           membership in @p all_dependencies.
 *
 * @return A YAML::Node of type Sequence.  Each element is a YAML map with
 *         the following keys:
 *         - "name"           (string)         — the option's identifier.
 *         - "description"    (string, optional) — human-readable explanation.
 *         - "enabled"        (bool, optional) — the default enabled state.
 *         - "enabled_value"  (string or null) — the value passed to the build
 *                                                system when enabled (null if
 *                                                the option is a boolean flag).
 *         - "disabled_value" (string or null, optional) — the value passed when
 *                                                disabled (only present when
 *                                                `disabled_format` is set in
 *                                                the source `BuildOption`).
 *         - "requires"       (sequence, optional) — the list of packages that
 *                                                must be present for this
 *                                                option to be valid.
 *
 * @note Options for which `user_configurable` is `false` are always excluded
 *       from the output, regardless of whether their requirements are met.
 *
 * @note If an option has no `enabled` default value, the output defaults to
 *       `true`.  If it has no `enabled_value` default, the output stores an
 *       explicit null node rather than the option's `enabled_format` string.
 *
 * @see BuildOption           Struct definition in database/config.hpp.
 * @see requirements_satisfied  The helper that evaluates requirement
 *                              expressions against the dependency set.
 * @see AbstractPackageSelections  Type alias for the abstract-to-concrete
 *                                 package mapping (resolve_dependencies.hpp).
 * @see config_transformer    Downstream component that consumes the filtered
 *                            options as part of user config generation.
 */
YAML::Node filtered_options(const std::vector<BuildOption>& options,
                            const std::vector<std::string>& all_dependencies,
                            const AbstractPackageSelections& abstract_packages);
