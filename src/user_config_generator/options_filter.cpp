#include <dependency_resolver/requirements.hpp>
#include <user_config_generator/options_filter.hpp>

/**
 * @brief Filter build options by user-configurability and dependency satisfaction.
 *
 * Iterates over every entry in @p options and retains only those that are both
 * marked as user-configurable AND whose requirement list is satisfied by the
 * resolved dependency set (see ::requirements_satisfied).  Options that fail
 * either check are silently dropped from the output.
 *
 * For each retained option the function builds a YAML map node with the
 * following keys, following a strict set of rules that mirror the option's
 * default-state semantics:
 *
 *   Key              | Condition                                      | Value
 *   -----------------|------------------------------------------------|-----------------------------
 *   `name`           | always present                                 | `option.name`
 *   `description`    | `option.description.has_value()`               | the description string
 *   `enabled`        | `!option.enabled.has_value()`                  | `true`
 *                    | `option.enabled->default_value.has_value()`    | the default boolean
 *                    | otherwise                                      | omitted
 *   `enabled_value`  | `!option.enabled_value.has_value()`            | explicit YAML null
 *                    | `option.enabled_value->default_value.has_value()` | the default value string
 *                    | otherwise                                      | omitted
 *   `disabled_value` | only emitted when `disabled_format.has_value()` | YAML null or default string
 *   `requires`       | `!option.requires.empty()`                     | the full requirement list
 *
 * The resulting YAML sequence is consumed by downstream stages of the
 * user-config generation pipeline (e.g. @c config_transformer and
 * @c user_config_generator) to produce the user-editable configuration file.
 *
 * @param options            The raw build options from a package recipe's
 *                           `BuildConfiguration`.  Each element is a
 *                           @c BuildOption carrying a name, optional
 *                           description, user-configurability flag, optional
 *                           default enable states, optional enabled/disabled
 *                           format strings, and an optional requirement list.
 * @param all_dependencies   The full list of concrete package names that the
 *                           dependency resolver has selected for installation.
 *                           Used to evaluate each option's @c requires field.
 *                           This is typically the union of essential and
 *                           optional dependencies produced by
 *                           @c resolve_dependencies().
 * @param abstract_packages  Mapping from abstract package names (e.g. "BLAS",
 *                           "LAPACK", "MPI") to their concrete implementation
 *                           selections (e.g. "openblas", "netlib-lapack",
 *                           "openmpi").  When an option's @c requires entry is
 *                           an abstract key, it is resolved through this map
 *                           before membership in @p all_dependencies is checked.
 *
 * @return A YAML::Node of type Sequence.  Each element is a YAML map
 *         containing the filtered option's metadata.  Returns an empty
 *         sequence if no options survive the filter.
 *
 * @note Options whose @c user_configurable field is @c false are always
 *       excluded, regardless of whether their requirements are satisfied.
 *       This is the primary gate — options are hidden from the user unless
 *       the recipe author explicitly opted them in.
 *
 * @note The `enabled` key defaults to @c true when the source option has no
 *       @c enabled value at all (i.e. the option is unconditionally active).
 *       This provides a sensible default in the generated config so that
 *       users see `enabled: true` rather than an absent field.
 *
 * @note When an option has an @c enabled_format but no @c enabled_value
 *       default, the output stores an explicit YAML null node for
 *       `enabled_value`.  Downstream template resolution interprets a null
 *       as a pure boolean flag (no value substitution in the format string).
 *
 * @note The @c disabled_value key is only emitted when the source option
 *       declares a @c disabled_format string; otherwise the option cannot
 *       be toggled off in a meaningful way, so no disabled-related keys
 *       appear in the output.
 *
 * @note This function does **not** call @c ERROR(), @c fail_config(), or
 *       @c user_config_error().  Options that fail the filter are silently
 *       skipped.  If the input list is empty the function returns an empty
 *       sequence without error.
 *
 * @see BuildOption               Struct definition in database/config.hpp.
 * @see requirements_satisfied    The helper that evaluates requirement
 *                                expressions against the dependency set.
 * @see AbstractPackageSelections Type alias for the abstract-to-concrete
 *                                package mapping (resolve_dependencies.hpp).
 * @see config_transformer        Downstream component that consumes the
 *                                filtered options during config generation.
 */
YAML::Node filtered_options(const std::vector<BuildOption>& options,
                            const std::vector<std::string>& all_dependencies,
                            const AbstractPackageSelections& abstract_packages) {
    YAML::Node result(YAML::NodeType::Sequence);
    for (const BuildOption& option : options) {
        // Skip options that are not user-configurable or whose requirements
        // are not met by the resolved dependency set.
        if (!option.user_configurable ||
            !requirements_satisfied(option.requires, all_dependencies, abstract_packages)) {
            continue;
        }

        YAML::Node output(YAML::NodeType::Map);
        output["name"] = option.name;
        if (option.description.has_value()) {
            output["description"] = *option.description;
        }

        // "enabled": default to true when the option has no conditional
        // enable/disable logic at all.
        if (!option.enabled.has_value()) {
            output["enabled"] = true;
        } else if (option.enabled->default_value.has_value()) {
            output["enabled"] = *option.enabled->default_value;
        }

        // "enabled_value": null when the option is a pure boolean flag
        // (no value to substitute into the format string).
        if (!option.enabled_value.has_value()) {
            output["enabled_value"] = YAML::Node(YAML::NodeType::Null);
        } else if (option.enabled_value->default_value.has_value()) {
            output["enabled_value"] = *option.enabled_value->default_value;
        }

        // "disabled_value": only emitted when the option supports a
        // disabled format (i.e. an explicit "off" representation).
        if (option.disabled_format.has_value()) {
            if (!option.disabled_value.has_value()) {
                output["disabled_value"] = YAML::Node(YAML::NodeType::Null);
            } else if (option.disabled_value->default_value.has_value()) {
                output["disabled_value"] = *option.disabled_value->default_value;
            }
        }

        // Expose the requirement list so downstream tooling knows which
        // packages must be present for this option to be valid.
        if (!option.requires.empty()) {
            output["requires"] = option.
                                     requires;
        }
        result.push_back(output);
    }
    return result;
}
