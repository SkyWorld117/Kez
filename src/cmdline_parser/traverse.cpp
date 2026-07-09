#include <cctype>
#include <cmdline_parser/traverse.hpp>
#include <cstdlib>
#include <limits>
#include <string>
#include <utils/colored_io.hpp>
#include <utils/yaml_utils.hpp>
#include <vector>

namespace {
    /**
     * @brief Print a fatal error message for an invalid configuration path and
     *        terminate the process.
     *
     * This is the common error handler for every malformed path encountered
     * during traversal.  It prints an "[E]:"-prefixed message via @c ERROR()
     * and immediately exits with a non-zero status.
     *
     * @param path  The offending configuration path string to include in the
     *              diagnostic message.
     *
     * @warning This function never returns.  It calls @c exit(EXIT_FAILURE).
     */
    [[noreturn]] void invalid_path(const std::string& path) {
        ERROR("Invalid configuration path: " + path);
        exit(EXIT_FAILURE);
    }

    /**
     * @brief Split a dot-separated configuration path into its component parts.
     *
     * Each segment between dots must be non-empty; an empty segment or an empty
     * input triggers a fatal error via invalid_path().
     */
    std::vector<std::string> split_path(const std::string& path) {
        if (path.empty()) {
            invalid_path(path);
        }

        std::vector<std::string> result;
        std::size_t begin = 0;
        while (begin <= path.size()) {
            const std::size_t end  = path.find('.', begin);
            const std::string part = path.substr(begin, end - begin);
            if (part.empty()) {
                invalid_path(path);
            }
            result.push_back(part);
            if (end == std::string::npos) {
                break;
            }
            begin = end + 1;
        }
        return result;
    }

    /**
     * @brief Determine whether a string is a non-negative decimal integer and
     *        parse it into a size_t.
     *
     * Returns false on empty strings, non-digit characters, or overflow beyond
     * SIZE_MAX.  On success stores the parsed value in @p result and returns
     * true.
     */
    bool numeric_index(const std::string& value, std::size_t& result) {
        if (value.empty()) {
            return false;
        }
        std::size_t parsed = 0;
        for (const char character : value) {
            if (!std::isdigit(static_cast<unsigned char>(character))) {
                return false;
            }
            const std::size_t digit = static_cast<std::size_t>(character - '0');
            // Check overflow: if parsed > (max - digit) / 10, then
            // parsed * 10 + digit would exceed SIZE_MAX.
            if (parsed > (std::numeric_limits<std::size_t>::max() - digit) / 10) {
                return false;
            }
            parsed = parsed * 10 + digit;
        }
        result = parsed;
        return true;
    }

    /**
     * @brief Resolve a path selector against a YAML sequence node and return
     *        the matching index.
     *
     * If @p selector is a decimal number it is treated as a literal position;
     * otherwise each sequence element is searched for a map with a "name" or
     * "target" key whose scalar value matches @p selector.  A fatal error is
     * raised when no match is found.
     */
    std::size_t sequence_index(const YAML::Node& node, const std::string& selector,
                               const std::string& path) {
        std::size_t index = 0;
        if (numeric_index(selector, index)) {
            if (index >= node.size()) {
                invalid_path(path);
            }
            return index;
        }

        for (std::size_t current = 0; current < node.size(); ++current) {
            const YAML::Node item = node[current];
            if (!item.IsMap()) {
                continue;
            }
            for (const std::string key : {"name", "target"}) {
                if (yaml_has(item, key) && item[key].IsScalar() && item[key].Scalar() == selector) {
                    return current;
                }
            }
        }
        invalid_path(path);
    }

    /**
     * @brief Recursively walk a parsed path and assign a value at the target
     *        YAML location.
     *
     * Operates on maps (descending by key) and sequences (resolving the
     * selector via sequence_index()).  A fatal error is raised if an
     * intermediate key is missing or the node type is unexpected.
     */
    void assign(const std::vector<std::string>& parts, std::size_t position,
                const std::string& value, YAML::Node node, const std::string& full_path) {
        if (node.IsMap()) {
            const std::string& key = parts[position];
            if (!node[key].IsDefined()) {
                invalid_path(full_path);
            }
            if (position + 1 == parts.size()) {
                node[key] = value;
                return;
            }
            assign(parts, position + 1, value, node[key], full_path);
            return;
        }

        if (node.IsSequence()) {
            const std::size_t index = sequence_index(node, parts[position], full_path);
            if (position + 1 == parts.size()) {
                node[index] = value;
                return;
            }
            assign(parts, position + 1, value, node[index], full_path);
            return;
        }

        invalid_path(full_path);
    }
}  // namespace

/**
 * @brief Parse a dot-separated configuration path and assign a string value at
 *        the target YAML node.
 *
 * This is the sole public entry point.  It splits @p path into parts and
 * delegates to the anonymous-namespace assign() helper.  A fatal error is
 * raised for any malformed or unresolvable path.
 */
void traverse(const std::string& path, const std::string& value, YAML::Node node) {
    const std::vector<std::string> parts = split_path(path);
    assign(parts, 0, value, node, path);
}
