#pragma once

#include <yaml-cpp/yaml.h>

#include <database/config.hpp>
#include <database/parser_context.hpp>
#include <string>

/**
 * @brief Parse a YAML node into a @c ConfigurableValue<bool>.
 *
 * Reads a mapping that may contain a @c default key (a boolean scalar) and an
 * optional @c conditions key (a sequence of condition maps).  Each condition
 * map must include a @c condition (a string expression), a @c value (a boolean
 * scalar), and an optional @c action.  Unlike the string counterpart, boolean
 * configurables only support the @c ValueAction::Set action; any other action
 * on any condition causes a fatal configuration error.
 *
 * At least one of @c default or @c conditions must be present; otherwise the
 * function terminates with a fatal message.
 *
 * @param node    The YAML node (expected to be a map) to parse.
 * @param path    Dot-separated YAML path used for error-reporting context
 *                (e.g. @c "build.configurations.options[0].enabled").
 * @param context The parser context providing the source file path for
 *                diagnostic messages.
 * @return A fully populated @c ConfigurableValue<bool> containing the parsed
 *         default value (if any) and the list of conditional values.
 *
 * @warning Terminates the process with @c exit(EXIT_FAILURE) if the node is
 *          not a map, contains unexpected keys, lacks both a default and any
 *          conditions, or a condition uses a non-Set action.
 *
 * @see parse_string_configurable
 * @see ConfigurableValue
 * @see ConditionalValue
 */
ConfigurableValue<bool> parse_bool_configurable(const YAML::Node& node, const std::string& path,
                                                const DatabaseParserContext& context);

/**
 * @brief Parse a YAML node into a @c ConfigurableValue<std::string>.
 *
 * Reads a mapping that may contain a @c default key (a string scalar) and an
 * optional @c conditions key (a sequence of condition maps).  Each condition
 * map must include a @c condition (a string expression), a @c value (a string
 * scalar), and an optional @c action, which may be @c set, @c append, or
 * @c prepend.
 *
 * At least one of @c default or @c conditions must be present; otherwise the
 * function terminates with a fatal message.
 *
 * @param node    The YAML node (expected to be a map) to parse.
 * @param path    Dot-separated YAML path used for error-reporting context
 *                (e.g. @c "build.configurations.options[0].enabled_value").
 * @param context The parser context providing the source file path for
 *                diagnostic messages.
 * @return A fully populated @c ConfigurableValue<std::string> containing the
 *         parsed default value (if any) and the list of conditional values.
 *
 * @warning Terminates the process with @c exit(EXIT_FAILURE) if the node is
 *          not a map, contains unexpected keys, or lacks both a default and
 *          any conditions.
 *
 * @see parse_bool_configurable
 * @see ConfigurableValue
 * @see ConditionalValue
 */
ConfigurableValue<std::string> parse_string_configurable(const YAML::Node& node,
                                                         const std::string& path,
                                                         const DatabaseParserContext& context);

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
 *     option names produce a warning but are retained.
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
                                             const DatabaseParserContext& context);

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
 *         @c "all" or @c "install").  May be null.
 *       - @c multithreaded (bool)          -- whether parallel jobs are
 *         allowed (default @c true).
 *       - @c configurations (map)          -- per-stage configuration
 *         overrides (command, environment, options).
 *
 * If the @c stages key is present, every entry is validated as a map with
 * only the allowed keys listed above.
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
                  const DatabaseParserContext& context);
