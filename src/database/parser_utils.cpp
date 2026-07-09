#include <cstdlib>
#include <database/parser_utils.hpp>
#include <unordered_set>
#include <utils/colored_io.hpp>
#include <utils/yaml_utils.hpp>

/**
 * @brief Reports a fatal configuration error and terminates the process.
 *
 * Constructs an error string from the source file path, the YAML mark
 * (line:column) of the offending node (if available), the logical
 * @p yaml_path, and the descriptive @p message. The message is printed
 * via ERROR() and the program exits with EXIT_FAILURE.
 *
 * @param node    The YAML node that caused the error. Its Mark() provides
 *                the line/column location. May be undefined or null-marked.
 * @param yaml_path Logical dot-separated path within the config schema
 *                  (e.g. "build/options").
 * @param message Human-readable explanation of the error.
 * @param context Carries the source file path for location annotation.
 *
 * @warning This function never returns -- it terminates the program.
 */
[[noreturn]] void fail_config(const YAML::Node& node, const std::string& yaml_path,
                              const std::string& message, const DatabaseParserContext& context) {
    std::string error_smg = context.source_path.string();
    if (node.IsDefined() && !node.Mark().is_null()) {
        error_smg += ':' + std::to_string(node.Mark().line + 1) + ':' +
                     std::to_string(node.Mark().column + 1);
    }
    error_smg += ": " + yaml_path + ' ' + message;

    ERROR(error_smg);
    exit(EXIT_FAILURE);
}

/**
 * @brief Emits a non-fatal configuration warning.
 *
 * Builds a warning string with the same location-annotated format as
 * fail_config, but uses WARNING() instead of ERROR() and does **not**
 * terminate the process. Intended for recoverable issues such as
 * unrecognised keys or deprecated fields.
 *
 * @param node    The YAML node the warning relates to.
 * @param yaml_path Logical path within the config schema.
 * @param message Human-readable warning description.
 * @param context Carries the source file path for location annotation.
 */
void warn_config(const YAML::Node& node, const std::string& yaml_path, const std::string& message,
                 const DatabaseParserContext& context) {
    std::string warning_msg = context.source_path.string();
    if (node.IsDefined() && !node.Mark().is_null()) {
        warning_msg += ':' + std::to_string(node.Mark().line + 1) + ':' +
                       std::to_string(node.Mark().column + 1);
    }
    warning_msg += ": " + yaml_path + ' ' + message;
    WARNING(warning_msg);
}

/**
 * @brief Asserts that a YAML node is a map (dictionary).
 *
 * If the node is not a map, fail_config is called with a message stating
 * that a map was expected, terminating the program.
 *
 * @param node    The YAML node to validate.
 * @param path    Logical path included in the error message for context.
 * @param context Parser context forwarded to fail_config.
 *
 * @warning Terminates the program if validation fails.
 */
void expect_map(const YAML::Node& node, const std::string& path,
                const DatabaseParserContext& context) {
    if (!node.IsMap()) {
        fail_config(node, path, "must be a map", context);
    }
}

/**
 * @brief Asserts that a YAML node is a sequence (list).
 *
 * If the node is not a sequence, fail_config is called with a message
 * stating that a sequence was expected, terminating the program.
 *
 * @param node    The YAML node to validate.
 * @param path    Logical path included in the error message for context.
 * @param context Parser context forwarded to fail_config.
 *
 * @warning Terminates the program if validation fails.
 */
void expect_sequence(const YAML::Node& node, const std::string& path,
                     const DatabaseParserContext& context) {
    if (!node.IsSequence()) {
        fail_config(node, path, "must be a sequence", context);
    }
}

/**
 * @brief Validates that every key in a map node belongs to an allowed set.
 *
 * First asserts that @p node is a map (via expect_map). Then iterates over
 * all entries and checks each key:
 *   - Non-scalar keys trigger a fatal error.
 *   - Duplicate keys trigger a fatal error.
 *   - Keys not present in @p allowed_keys trigger a **warning** (not an
 *     error), so that recipe files written against a newer schema version
 *     remain forward-compatible.
 *
 * @param node         The map node whose keys are to be validated.
 * @param allowed_keys Whitelist of key names considered valid.
 * @param path         Logical path prefix for error/warning messages.
 * @param context      Parser context forwarded to expect_map, fail_config,
 *                     and warn_config.
 *
 * @warning Terminates the program if the node is not a map, contains a
 *          non-scalar key, or has duplicate keys.
 */
void check_keys(const YAML::Node& node, std::initializer_list<const char*> allowed_keys,
                const std::string& path, const DatabaseParserContext& context) {
    expect_map(node, path, context);
    std::unordered_set<std::string> allowed;
    for (const char* key : allowed_keys) {
        allowed.emplace(key);
    }

    std::unordered_set<std::string> seen;
    for (const auto& entry : node) {
        if (!entry.first.IsScalar()) {
            fail_config(entry.first, path, "contains a non-scalar key", context);
        }
        const std::string key = entry.first.Scalar();
        if (!seen.emplace(key).second) {
            fail_config(entry.first, path + "." + key, "is duplicated", context);
        }
        if (allowed.find(key) == allowed.end()) {
            warn_config(entry.first, path + "." + key, "ignoring unexpected key", context);
        }
    }
}

/**
 * @brief Retrieves a required child node from a map by key.
 *
 * If @p key does not exist in @p map, the program terminates with a message
 * indicating that the key is required. Otherwise the associated YAML node
 * is returned as-is (no type validation is performed here).
 *
 * @param map     The parent map node to search.
 * @param key     The key to look up.
 * @param path    Logical path prefix used in the error message
 *                (the key is appended as ".key").
 * @param context Parser context forwarded to fail_config.
 *
 * @return The child YAML node at map[key].
 *
 * @warning Terminates the program if the key is missing.
 */
YAML::Node required_node(const YAML::Node& map, const std::string& key, const std::string& path,
                         const DatabaseParserContext& context) {
    if (!yaml_has(map, key)) {
        fail_config(map, path + "." + key, "is required", context);
    }
    return map[key];
}

/**
 * @brief Parses a YAML node as a scalar string with an optional null allowance.
 *
 * Validates that the node is a scalar (or null when @p allow_null is true).
 * A null node under allow_null is returned as an empty string. Non-scalar
 * values always trigger a fatal error.
 *
 * @param node      The YAML node to parse.
 * @param path      Logical path used in the error message.
 * @param context   Parser context forwarded to fail_config.
 * @param allow_null If true, a null node is accepted and returned as "".
 *
 * @return The scalar value as a std::string (empty string for null).
 *
 * @warning Terminates the program if the node is neither a scalar nor a
 *          permitted null.
 */
std::string parse_scalar(const YAML::Node& node, const std::string& path,
                         const DatabaseParserContext& context, bool allow_null) {
    if (node.IsNull() && allow_null) {
        return "";
    }
    if (!node.IsScalar()) {
        fail_config(node, path, allow_null ? "must be a scalar or null" : "must be a scalar",
                    context);
    }
    return node.Scalar();
}

/**
 * @brief Shorthand: retrieves a required key and parses its value as a scalar.
 *
 * Combines required_node and parse_scalar into one call. The key is looked
 * up in @p map and the child node is immediately parsed as a scalar string.
 *
 * @param map     The parent map node.
 * @param key     The key to look up.
 * @param path    Logical path prefix for error messages.
 * @param context Parser context forwarded to required_node and parse_scalar.
 *
 * @return The scalar value as a std::string.
 *
 * @warning Terminates the program if the key is missing or the value is not
 *          a scalar.
 *
 * @see required_node
 * @see parse_scalar
 */
std::string required_scalar(const YAML::Node& map, const std::string& key, const std::string& path,
                            const DatabaseParserContext& context) {
    return parse_scalar(required_node(map, key, path, context), path + "." + key, context);
}

/**
 * @brief Shorthand: retrieves an optional key and parses its value as a scalar.
 *
 * If @p key exists in @p map, the child node is parsed as a scalar string
 * and returned wrapped in std::optional. If the key is absent, std::nullopt
 * is returned. Note that a present but null/empty value is returned as an
 * empty string (via parse_scalar), not as std::nullopt.
 *
 * @param map     The parent map node.
 * @param key     The key to look up.
 * @param path    Logical path prefix for error messages.
 * @param context Parser context forwarded to parse_scalar.
 *
 * @return The scalar value wrapped in std::optional, or std::nullopt when
 *         the key is absent.
 *
 * @warning Terminates the program if the key exists but its value is not a
 *          scalar.
 */
std::optional<std::string> optional_scalar(const YAML::Node& map, const std::string& key,
                                           const std::string& path,
                                           const DatabaseParserContext& context) {
    if (!yaml_has(map, key)) {
        return std::nullopt;
    }
    return parse_scalar(map[key], path + "." + key, context);
}

/**
 * @brief Parses a YAML node as a boolean value.
 *
 * Delegates to parse_scalar to obtain the raw string, then compares it
 * against the exact strings "true" and "false". If neither matches, the
 * program terminates with an error message.
 *
 * @param node    The YAML node to parse.
 * @param path    Logical path used in error messages.
 * @param context Parser context forwarded to parse_scalar and fail_config.
 *
 * @return true if the scalar equals "true", false if it equals "false".
 *
 * @warning Terminates the program if the scalar is neither "true" nor
 *          "false", or if the node is not a scalar.
 */
bool parse_boolean(const YAML::Node& node, const std::string& path,
                   const DatabaseParserContext& context) {
    const std::string value = parse_scalar(node, path, context);
    if (value == "true") {
        return true;
    }
    if (value == "false") {
        return false;
    }
    fail_config(node, path, "must be either true or false", context);
}

/**
 * @brief Parses a YAML node as a ValueAction enum value.
 *
 * Delegates to parse_scalar to obtain the raw string, then maps it to the
 * corresponding ValueAction enumerator:
 *   - "set"    -> ValueAction::Set
 *   - "append" -> ValueAction::Append
 *   - "prepend" -> ValueAction::Prepend
 *
 * If the string does not match any of the three known actions, the program
 * terminates with a message listing the valid options.
 *
 * @param node    The YAML node whose scalar value names the action.
 * @param path    Logical path used in error messages.
 * @param context Parser context forwarded to parse_scalar and fail_config.
 *
 * @return The matching ValueAction enumerator.
 *
 * @warning Terminates the program if the value is not one of "set",
 *          "append", or "prepend", or if the node is not a scalar.
 */
ValueAction parse_action(const YAML::Node& node, const std::string& path,
                         const DatabaseParserContext& context) {
    const std::string value = parse_scalar(node, path, context);
    if (value == "set") {
        return ValueAction::Set;
    }
    if (value == "append") {
        return ValueAction::Append;
    }
    if (value == "prepend") {
        return ValueAction::Prepend;
    }
    fail_config(node, path, "must be one of set, append, or prepend", context);
}
