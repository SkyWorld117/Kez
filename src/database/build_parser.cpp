#include <database/build_parser.hpp>
#include <database/condition_parser.hpp>
#include <database/parser_utils.hpp>
#include <database/source_parser.hpp>
#include <unordered_set>
#include <utility>
#include <utils/yaml_utils.hpp>

namespace {
    /**
     * @brief Parse a generic YAML node into a @c ConfigurableValue<T>.
     *
     * Reads a mapping that must contain either a @c default key or a @c conditions
     * key (or both).  The @c default key, when present and non-null, is parsed by
     * the caller-supplied @p parse_value callback.  The @c conditions key, when
     * present, must be a sequence of maps, each containing:
     *   - @c condition  (string) -- an expression evaluated at resolution time.
     *   - @c value      (T)      -- the value to apply when the condition matches.
     *   - @c action     (string) -- optional; how to combine with the default
     *     (parsed via @c parse_action; defaults to @c ValueAction::Set).
     *
     * At least one of @c default or @c conditions must be present; otherwise the
     * function terminates with a fatal error via @c fail_config.
     *
     * @tparam T          The value type (e.g. @c bool, @c std::string).
     * @tparam ValueParser Callable type invoked as
     *                    @c T(const YAML::Node& value_node, const std::string& value_path).
     * @param node        The YAML node (expected to be a map) to parse.
     * @param path        Dot-separated YAML path used for error-reporting context
     *                    (e.g. @c "build.configurations.options[0].enabled").
     * @param context     The parser context providing the source file path for
     *                    diagnostic messages.
     * @param parse_value Callback that converts a YAML node into the target type
     *                    @c T.  Called for both the default value and each
     *                    condition value.
     * @return A fully populated @c ConfigurableValue<T> containing the parsed
     *         default value (if any) and the list of conditional values.
     *
     * @warning Terminates the process with @c exit(EXIT_FAILURE) if @p node is
     *          not a map, contains unexpected keys, or lacks both a default and
     *          any conditions.
     *
     * @see ConfigurableValue
     * @see ConditionalValue
     * @see parse_bool_configurable
     * @see parse_string_configurable
     */
    template <typename T, typename ValueParser>
    ConfigurableValue<T> parse_configurable(const YAML::Node& node, const std::string& path,
                                            const DatabaseParserContext& context,
                                            ValueParser parse_value) {
        expect_map(node, path, context);
        check_keys(node, {"default", "conditions"}, path, context);
        ConfigurableValue<T> result;
        if (yaml_has(node, "default") && !node["default"].IsNull()) {
            result.default_value = parse_value(node["default"], path + ".default");
        }
        if (yaml_has(node, "conditions")) {
            YAML::Node conditions = node["conditions"];
            expect_sequence(conditions, path + ".conditions", context);
            for (std::size_t i = 0; i < conditions.size(); ++i) {
                const std::string condition_path = path + ".conditions[" + std::to_string(i) + "]";
                YAML::Node condition_node        = conditions[i];
                expect_map(condition_node, condition_path, context);
                check_keys(condition_node, {"condition", "action", "value"}, condition_path,
                           context);

                ConditionalValue<T> condition;
                condition.condition =
                    required_scalar(condition_node, "condition", condition_path, context);
                validate_condition(condition.condition, condition_node["condition"],
                                   condition_path + ".condition", context);
                if (yaml_has(condition_node, "action")) {
                    condition.action =
                        parse_action(condition_node["action"], condition_path + ".action", context);
                }
                condition.value =
                    parse_value(required_node(condition_node, "value", condition_path, context),
                                condition_path + ".value");
                result.conditions.push_back(std::move(condition));
            }
        }
        if (!result.default_value.has_value() && result.conditions.empty()) {
            fail_config(node, path, "must define a default or at least one condition", context);
        }
        return result;
    }

}  // namespace

/**
 * @brief Parse a YAML node into a @c ConfigurableValue<bool>.
 *
 * Delegates to @c parse_configurable<bool> with a value parser that calls
 * @c parse_boolean, then validates that every condition uses the
 * @c ValueAction::Set action.  Any condition with a non-Set action triggers
 * a fatal configuration error because appending or prepending to a boolean
 * is semantically meaningless.
 *
 * @param node    The YAML node (expected to be a map) to parse.
 * @param path    Dot-separated YAML path used for error-reporting context
 *                (e.g. @c "build.configurations.options[0].enabled").
 * @param context The parser context providing the source file path for
 *                diagnostic messages.
 * @return A fully populated @c ConfigurableValue<bool> containing the parsed
 *         default value (if any) and the list of conditional values.
 *
 * @warning Terminates the process with @c exit(EXIT_FAILURE) if @p node is
 *          not a map, contains unexpected keys, lacks both a default and any
 *          conditions, or a condition uses a non-Set action.
 *
 * @see parse_configurable
 * @see parse_string_configurable
 */
ConfigurableValue<bool> parse_bool_configurable(const YAML::Node& node, const std::string& path,
                                                const DatabaseParserContext& context) {
    ConfigurableValue<bool> result = parse_configurable<bool>(
        node, path, context, [&context](const YAML::Node& value, const std::string& value_path) {
            return parse_boolean(value, value_path, context);
        });
    for (const auto& condition : result.conditions) {
        if (condition.action != ValueAction::Set) {
            fail_config(node, path, "boolean conditions only support the set action", context);
        }
    }
    return result;
}

/**
 * @brief Parse a YAML node into a @c ConfigurableValue<std::string>.
 *
 * Delegates to @c parse_configurable<std::string> with a value parser that
 * calls @c parse_scalar (with @c allow_null=true).  Unlike the boolean
 * variant, string configurables permit all @c ValueAction modes (Set,
 * Append, Prepend).
 *
 * @param node    The YAML node (expected to be a map) to parse.
 * @param path    Dot-separated YAML path used for error-reporting context
 *                (e.g. @c "build.configurations.options[0].enabled_value").
 * @param context The parser context providing the source file path for
 *                diagnostic messages.
 * @return A fully populated @c ConfigurableValue<std::string> containing the
 *         parsed default value (if any) and the list of conditional values.
 *
 * @warning Terminates the process with @c exit(EXIT_FAILURE) if @p node is
 *          not a map, contains unexpected keys, or lacks both a default and
 *          any conditions.
 *
 * @see parse_configurable
 * @see parse_bool_configurable
 */
ConfigurableValue<std::string> parse_string_configurable(const YAML::Node& node,
                                                         const std::string& path,
                                                         const DatabaseParserContext& context) {
    return parse_configurable<std::string>(
        node, path, context, [&context](const YAML::Node& value, const std::string& value_path) {
            return parse_scalar(value, value_path, context, true);
        });
}

namespace {
    /**
     * @brief Parse a YAML node into an @c EnvironmentVariable.
     *
     * Reads a mapping with the following recognized keys:
     *   - @c name              (string, required) -- the variable name.
     *   - @c description       (string, optional) -- human-readable description.
     *   - @c user_configurable (bool,   optional) -- whether end-users may
     *     override this variable (default: @c false).
     *   - @c requires          (sequence of strings, optional) -- packages that
     *     must be resolvable before this variable can be evaluated.
     *   - @c default           (string, optional) -- the variable's default
     *     value when no condition matches.
     *   - @c conditions        (sequence of maps, optional) -- condition-
     *     dependent overrides.  If both @c default and @c conditions are
     *     present, they are wrapped together into a single
     *     @c ConfigurableValue<std::string> via
     *     @c parse_string_configurable.
     *
     * @param node    The YAML node (expected to be a map) to parse.
     * @param path    Dot-separated YAML path used for error-reporting context
     *                (e.g. @c "build.configurations.environment[0]").
     * @param context The parser context providing the source file path for
     *                diagnostic messages.
     * @return A fully populated @c EnvironmentVariable with the parsed name,
     *         metadata, and configurable value.
     *
     * @warning Terminates the process with @c exit(EXIT_FAILURE) if the node is
     *          not a map, contains unexpected keys, or a required key is missing.
     *
     * @see EnvironmentVariable
     * @see parse_string_configurable
     */
    EnvironmentVariable parse_environment_variable(const YAML::Node& node, const std::string& path,
                                                   const DatabaseParserContext& context) {
        expect_map(node, path, context);
        check_keys(
            node, {"name", "description", "user_configurable", "requires", "default", "conditions"},
            path, context);

        EnvironmentVariable result;
        result.name        = required_scalar(node, "name", path, context);
        result.description = optional_scalar(node, "description", path, context);
        if (yaml_has(node, "user_configurable")) {
            result.user_configurable =
                parse_boolean(node["user_configurable"], path + ".user_configurable", context);
        }
        if (yaml_has(node, "requires")) {
            result.
                requires
            = parse_scalar_sequence(node["requires"], path + ".requires", context);
        }
        if (yaml_has(node, "default") && !node["default"].IsNull()) {
            result.value.default_value = parse_scalar(node["default"], path + ".default", context);
        }
        if (yaml_has(node, "conditions")) {
            /* Wrap default + conditions into a synthetic map so that
               parse_string_configurable can parse them as a single unit. */
            YAML::Node wrapper(YAML::NodeType::Map);
            if (yaml_has(node, "default")) {
                wrapper["default"] = node["default"];
            }
            wrapper["conditions"] = node["conditions"];
            result.value          = parse_string_configurable(wrapper, path, context);
        }
        return result;
    }

    /**
     * @brief Parse a YAML node into a @c BuildOption.
     *
     * Reads a mapping with the following recognized keys:
     *   - @c name              (string, required) -- short option name.
     *   - @c description       (string, optional) -- human-readable description.
     *   - @c user_configurable (bool,   optional) -- whether end-users may
     *     override this option (default: @c false).
     *   - @c enabled           (map,    optional) -- condition-dependent boolean
     *     control for whether the option is enabled.  Parsed via
     *     @c parse_bool_configurable.
     *   - @c enabled_format    (string, optional) -- format string used when
     *     the option is enabled.
     *   - @c disabled_format   (string, optional) -- format string used when
     *     the option is disabled.
     *   - @c requires          (sequence of strings, optional) -- packages that
     *     must be present for this option to be valid.
     *   - @c enabled_value     (map,    optional) -- condition-dependent string
     *     value applied when the option is enabled.  Parsed via
     *     @c parse_string_configurable.
     *   - @c disabled_value    (map,    optional) -- condition-dependent string
     *     value applied when the option is disabled.  Parsed via
     *     @c parse_string_configurable.
     *
     * @param node    The YAML node (expected to be a map) to parse.
     * @param path    Dot-separated YAML path used for error-reporting context
     *                (e.g. @c "build.configurations.options[0]").
     * @param context The parser context providing the source file path for
     *                diagnostic messages.
     * @return A fully populated @c BuildOption with the parsed name, metadata,
     *         and configurable values.
     *
     * @warning Terminates the process with @c exit(EXIT_FAILURE) if the node is
     *          not a map or contains unrecognized keys.
     *
     * @see BuildOption
     * @see parse_bool_configurable
     * @see parse_string_configurable
     */
    BuildOption parse_option(const YAML::Node& node, const std::string& path,
                             const DatabaseParserContext& context) {
        expect_map(node, path, context);
        check_keys(node,
                   {"name", "description", "user_configurable", "enabled", "enabled_format",
                    "disabled_format", "requires", "enabled_value", "disabled_value"},
                   path, context);

        BuildOption result;
        result.name        = required_scalar(node, "name", path, context);
        result.description = optional_scalar(node, "description", path, context);
        if (yaml_has(node, "user_configurable")) {
            result.user_configurable =
                parse_boolean(node["user_configurable"], path + ".user_configurable", context);
        }
        if (yaml_has(node, "enabled")) {
            result.enabled = parse_bool_configurable(node["enabled"], path + ".enabled", context);
        }
        result.enabled_format  = optional_scalar(node, "enabled_format", path, context);
        result.disabled_format = optional_scalar(node, "disabled_format", path, context);
        if (yaml_has(node, "requires")) {
            result.
                requires
            = parse_scalar_sequence(node["requires"], path + ".requires", context);
        }
        if (yaml_has(node, "enabled_value")) {
            result.enabled_value =
                parse_string_configurable(node["enabled_value"], path + ".enabled_value", context);
        }
        if (yaml_has(node, "disabled_value")) {
            result.disabled_value = parse_string_configurable(node["disabled_value"],
                                                              path + ".disabled_value", context);
        }
        return result;
    }
}  // namespace

/**
 * @brief Parse a YAML node into a @c BuildConfiguration.
 *
 * Reads an optional map with the following recognized keys:
 *   - @c command      (string)  -- the build-system command to invoke.
 *   - @c environment  (sequence of maps) -- environment variables to set
 *     during the build step.  Each entry is parsed by the internal
 *     @c parse_environment_variable helper.
 *   - @c options      (sequence of maps) -- user-configurable build options.
 *     Each entry is parsed by the internal @c parse_option helper.  Duplicate
 *     option names produce a warning (via @c warn_config) but are retained.
 *
 * @param node    The YAML node (expected to be a map) to parse.
 * @param path    Dot-separated YAML path used for error-reporting context
 *                (e.g. @c "build.configurations").
 * @param context The parser context providing the source file path for
 *                diagnostic messages.
 * @return A @c BuildConfiguration with the parsed command, environment
 *         variables, and options.  Unset optional fields are left
 *         disengaged.
 *
 * @warning Terminates the process with @c exit(EXIT_FAILURE) if the node is
 *          not a map or contains unrecognized keys.
 *
 * @see parse_build
 * @see BuildConfiguration
 * @see EnvironmentVariable
 * @see BuildOption
 */
BuildConfiguration parse_build_configuration(const YAML::Node& node, const std::string& path,
                                             const DatabaseParserContext& context) {
    expect_map(node, path, context);
    check_keys(node, {"command", "environment", "options"}, path, context);

    BuildConfiguration result;
    result.command = optional_scalar(node, "command", path, context);
    if (yaml_has(node, "environment")) {
        YAML::Node environment = node["environment"];
        expect_sequence(environment, path + ".environment", context);
        for (std::size_t i = 0; i < environment.size(); ++i) {
            EnvironmentVariable variable = parse_environment_variable(
                environment[i], path + ".environment[" + std::to_string(i) + "]", context);
            result.environment.push_back(std::move(variable));
        }
    }
    if (yaml_has(node, "options")) {
        YAML::Node options = node["options"];
        expect_sequence(options, path + ".options", context);
        std::unordered_set<std::string> names;
        for (std::size_t i = 0; i < options.size(); ++i) {
            BuildOption option =
                parse_option(options[i], path + ".options[" + std::to_string(i) + "]", context);
            if (!names.emplace(option.name).second) {
                warn_config(options[i], path + ".options",
                            "contains duplicate option name '" + option.name +
                                "'; both entries are retained",
                            context);
            }
            result.options.push_back(std::move(option));
        }
    }
    return result;
}

/**
 * @brief Parse a YAML node into a @c Build description.
 *
 * Reads an optional map with the following recognized keys:
 *   - @c preprocessing  (string) -- a shell command run before any stage.
 *   - @c postprocessing (string) -- a shell command run after all stages.
 *   - @c configurations (map)    -- build-wide configuration (command,
 *     environment, options).  Delegates to
 *     @c parse_build_configuration.
 *   - @c stages         (sequence of maps) -- ordered build stages.  Each
 *     stage map may contain:
 *       - @c target        (string)        -- the build target (e.g.
 *         @c "all" or @c "install").  May be null, in which case the
 *         target is left disengaged.
 *       - @c multithreaded (bool)          -- whether parallel jobs are
 *         allowed (default @c true).
 *       - @c configurations (map)          -- per-stage configuration
 *         overrides (command, environment, options).
 *
 * If the @c stages key is present, every entry is validated as a map with
 * only the allowed keys listed above.  The @c target key is required for
 * each stage (via @c required_node), but its value is permitted to be null
 * to indicate the toolchain default.
 *
 * @param node    The YAML node (expected to be a map) to parse.
 * @param path    Dot-separated YAML path used for error-reporting context
 *                (e.g. @c "build").
 * @param context The parser context providing the source file path for
 *                diagnostic messages.
 * @return A @c Build with the parsed preprocessing/postprocessing scripts,
 *         build-wide configuration, and ordered list of stages.
 *
 * @warning Terminates the process with @c exit(EXIT_FAILURE) if the node is
 *          not a map, contains unrecognized keys, or a stage entry is
 *          malformed.
 *
 * @see parse_build_configuration
 * @see Build
 * @see BuildStage
 */
Build parse_build(const YAML::Node& node, const std::string& path,
                  const DatabaseParserContext& context) {
    expect_map(node, path, context);
    check_keys(node, {"preprocessing", "postprocessing", "configurations", "stages"}, path,
               context);

    Build result;
    result.preprocessing  = optional_scalar(node, "preprocessing", path, context);
    result.postprocessing = optional_scalar(node, "postprocessing", path, context);
    if (yaml_has(node, "configurations")) {
        result.configurations =
            parse_build_configuration(node["configurations"], path + ".configurations", context);
    }
    if (yaml_has(node, "stages")) {
        YAML::Node stages = node["stages"];
        expect_sequence(stages, path + ".stages", context);
        for (std::size_t i = 0; i < stages.size(); ++i) {
            const std::string stage_path = path + ".stages[" + std::to_string(i) + "]";
            YAML::Node stage_node        = stages[i];
            expect_map(stage_node, stage_path, context);
            check_keys(stage_node, {"target", "multithreaded", "configurations"}, stage_path,
                       context);

            YAML::Node target = required_node(stage_node, "target", stage_path, context);
            BuildStage stage;
            if (!target.IsNull()) {
                stage.target = parse_scalar(target, stage_path + ".target", context);
            }
            if (yaml_has(stage_node, "multithreaded")) {
                stage.multithreaded = parse_boolean(stage_node["multithreaded"],
                                                    stage_path + ".multithreaded", context);
            }
            if (yaml_has(stage_node, "configurations")) {
                stage.configurations = parse_build_configuration(
                    stage_node["configurations"], stage_path + ".configurations", context);
            }
            result.stages.push_back(std::move(stage));
        }
    }
    return result;
}
