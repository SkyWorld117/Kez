#pragma once

#include <yaml-cpp/yaml.h>

#include <database/config.hpp>
#include <database/parser_context.hpp>
#include <initializer_list>
#include <optional>
#include <string>

/**
 * @brief Reports a fatal configuration error and terminates the program.
 *
 * Prints an error message annotated with the source location (YAML mark line/column
 * of the offending node), the file path being parsed, and the explanatory message.
 * The function never returns.
 *
 * @param node    The YAML node that triggered the error. Its Mark is used to
 *                pinpoint the location in the source file.
 * @param yaml_path  Logical path (e.g. "source/url") describing where in the
 *                   configuration the error occurred. Not necessarily a filesystem path.
 * @param message Human-readable description of what went wrong.
 * @param context Parser context carrying the source file path for annotation.
 */
[[noreturn]] void fail_config(const YAML::Node& node, const std::string& yaml_path,
                              const std::string& message, const DatabaseParserContext& context);

/**
 * @brief Emits a non-fatal configuration warning.
 *
 * Prints a warning message annotated with the source location and logical path,
 * but does **not** terminate the program. Used for recoverable issues such as
 * unknown keys or deprecated fields.
 *
 * @param node    The YAML node that the warning pertains to.
 * @param yaml_path  Logical path within the configuration.
 * @param message Human-readable warning description.
 * @param context Parser context carrying the source file path for annotation.
 */
void warn_config(const YAML::Node& node, const std::string& yaml_path, const std::string& message,
                 const DatabaseParserContext& context);

/**
 * @brief Asserts that a YAML node is a map (dictionary).
 *
 * If the node is not a map, calls fail_config to terminate with an appropriate
 * error message.
 *
 * @param node    The YAML node to check.
 * @param path    Logical path used in the error message for context.
 * @param context Parser context for error reporting.
 */
void expect_map(const YAML::Node& node, const std::string& path,
                const DatabaseParserContext& context);

/**
 * @brief Asserts that a YAML node is a sequence (list).
 *
 * If the node is not a sequence, calls fail_config to terminate with an
 * appropriate error message.
 *
 * @param node    The YAML node to check.
 * @param path    Logical path used in the error message for context.
 * @param context Parser context for error reporting.
 */
void expect_sequence(const YAML::Node& node, const std::string& path,
                     const DatabaseParserContext& context);

/**
 * @brief Verifies that a map node contains no keys outside an allowed set.
 *
 * Iterates over all keys present in @p node. Any key not present in
 * @p allowed_keys triggers a warning via warn_config. Unknown keys are
 * non-fatal so that forward-compatible recipe files parse without error.
 *
 * @param node         The map node whose keys are to be validated.
 * @param allowed_keys List of key names considered valid for this node.
 * @param path         Logical path used in warning messages.
 * @param context      Parser context for error/warning reporting.
 */
void check_keys(const YAML::Node& node, std::initializer_list<const char*> allowed_keys,
                const std::string& path, const DatabaseParserContext& context);

/**
 * @brief Retrieves a child node by key from a map, failing if the key is missing.
 *
 * Looks up @p key in the map @p map. If the key does not exist, the program
 * terminates with an error message indicating which required key is missing.
 *
 * @param map     The parent map node.
 * @param key     The key to look up.
 * @param path    Logical path prefix used in error messages (e.g. "build/stages[0]").
 * @param context Parser context for error reporting.
 * @return The child YAML node associated with @p key.
 */
YAML::Node required_node(const YAML::Node& map, const std::string& key, const std::string& path,
                         const DatabaseParserContext& context);

/**
 * @brief Parses a YAML scalar node into a std::string.
 *
 * Validates that @p node is a scalar (or, when @p allow_null is true, a null
 * node which is returned as an empty string). If the node is neither a scalar
 * nor a permitted null, the program terminates.
 *
 * @param node      The YAML node to parse.
 * @param path      Logical path used in error messages.
 * @param context   Parser context for error reporting.
 * @param allow_null If true, a null/undefined node is accepted and returned as
 *                   an empty string. Defaults to false.
 * @return The scalar value as a std::string.
 */
std::string parse_scalar(const YAML::Node& node, const std::string& path,
                         const DatabaseParserContext& context, bool allow_null = false);

/**
 * @brief Extracts a required key from a map and parses it as a scalar string.
 *
 * Shorthand for a required_node call followed by parse_scalar on the result.
 *
 * @param map     The parent map node.
 * @param key     The key whose value must be a scalar string.
 * @param path    Logical path prefix for error messages.
 * @param context Parser context for error reporting.
 * @return The scalar value as a std::string.
 *
 * @see required_node
 * @see parse_scalar
 */
std::string required_scalar(const YAML::Node& map, const std::string& key, const std::string& path,
                            const DatabaseParserContext& context);

/**
 * @brief Optionally extracts a key from a map and parses it as a scalar string.
 *
 * If @p key exists in @p map, its value is parsed as a scalar and returned.
 * If the key is absent, std::nullopt is returned. A null/empty value for an
 * existing key is returned as an empty string, **not** as std::nullopt.
 *
 * @param map     The parent map node.
 * @param key     The key to look up.
 * @param path    Logical path prefix for error messages.
 * @param context Parser context for error reporting.
 * @return The scalar value wrapped in std::optional, or std::nullopt if the
 *         key is absent.
 */
std::optional<std::string> optional_scalar(const YAML::Node& map, const std::string& key,
                                           const std::string& path,
                                           const DatabaseParserContext& context);

/**
 * @brief Parses a YAML node as a boolean value.
 *
 * Accepts the string values ``"true"``, ``"True"``, ``"TRUE"`` for true and
 * ``"false"``, ``"False"``, ``"FALSE"`` for false.  Unlike the standard YAML
 * boolean type, this function operates on the raw scalar string and does
 * **not** accept ``yes``/``no``/``on``/``off``.
 *
 * If the node cannot be interpreted as a boolean, the program terminates with
 * an error message.
 *
 * @param node    The YAML node to parse.
 * @param path    Logical path used in error messages.
 * @param context Parser context for error reporting.
 * @return true or false as determined by the string content.
 */
bool parse_boolean(const YAML::Node& node, const std::string& path,
                   const DatabaseParserContext& context);

/**
 * @brief Parses a YAML node as a ValueAction enum.
 *
 * Interprets the scalar value of @p node and maps it to the corresponding
 * ValueAction enumerator:
 *   - "set"    -> ValueAction::Set
 *   - "append" -> ValueAction::Append
 *   - "prepend" -> ValueAction::Prepend
 *
 * If the value does not match any known action, the program terminates with
 * an error message describing the valid options.
 *
 * @param node    The YAML node whose scalar value names the action.
 * @param path    Logical path used in error messages.
 * @param context Parser context for error reporting.
 * @return The corresponding ValueAction enumerator.
 *
 * @see ValueAction
 */
ValueAction parse_action(const YAML::Node& node, const std::string& path,
                         const DatabaseParserContext& context);
