#include <cstdlib>
#include <database/parser_utils.hpp>
#include <unordered_set>
#include <utils/colored_io.hpp>
#include <utils/yaml_utils.hpp>

/** @brief Terminates the program with an error message pointing to the exact YAML location and the given message. */
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

/** @brief Prints a configuration warning with source location to stderr without terminating. */
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

/** @brief Asserts that the given YAML node is a map, otherwise fails with a formatted error. */
void expect_map(const YAML::Node& node, const std::string& path,
                const DatabaseParserContext& context) {
    if (!node.IsMap()) {
        fail_config(node, path, "must be a map", context);
    }
}

/** @brief Asserts that the given YAML node is a sequence, otherwise fails with a formatted error. */
void expect_sequence(const YAML::Node& node, const std::string& path,
                     const DatabaseParserContext& context) {
    if (!node.IsSequence()) {
        fail_config(node, path, "must be a sequence", context);
    }
}

/** @brief Validates that every key in the map is from an allowed set and that no key is duplicated; warns on unexpected keys. */
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

/** @brief Returns the child node for a required key, failing if the key is absent from the map. */
YAML::Node required_node(const YAML::Node& map, const std::string& key, const std::string& path,
                         const DatabaseParserContext& context) {
    if (!yaml_has(map, key)) {
        fail_config(map, path + "." + key, "is required", context);
    }
    return map[key];
}

/** @brief Extracts and returns the scalar string value from a node, optionally allowing null (returns empty string). */
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

/** @brief Shortcut: requires a map key and parses its value as a scalar string. */
std::string required_scalar(const YAML::Node& map, const std::string& key, const std::string& path,
                            const DatabaseParserContext& context) {
    return parse_scalar(required_node(map, key, path, context), path + "." + key, context);
}

/** @brief Optionally reads a scalar string from a map entry, returning std::nullopt when the key is absent. */
std::optional<std::string> optional_scalar(const YAML::Node& map, const std::string& key,
                                           const std::string& path,
                                           const DatabaseParserContext& context) {
    if (!yaml_has(map, key)) {
        return std::nullopt;
    }
    return parse_scalar(map[key], path + "." + key, context);
}

/** @brief Parses a YAML scalar node as a boolean ("true" / "false"), failing on any other value. */
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

/** @brief Parses a YAML scalar node into a ValueAction enum (set / append / prepend), failing on any other value. */
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
