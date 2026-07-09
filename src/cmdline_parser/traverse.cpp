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
     * @brief Split a dot-separated path string into its individual segments.
     *
     * Parses a path of the form \c "foo.bar.baz" into a vector
     * \c {"foo", "bar", "baz"}.  Empty segments (caused by leading,
     * trailing, or consecutive dots) are treated as invalid and trigger
     * program termination via @ref invalid_path.
     *
     * @param path  The dot-separated path to split.  Must be non-empty and
     *              must not contain empty segments.
     * @return A vector of segment strings.  The vector is guaranteed to
     *         contain at least one element on success.
     *
     * @warning Terminates the process via @ref invalid_path if @p path is
     *          empty or if any segment is empty.
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
     * @brief Attempt to parse a string as a non-negative @c std::size_t
     *        integer.
     *
     * Accepts only decimal digits (0--9).  Overflow is detected: if the
     * parsed value would exceed @c std::numeric_limits<std::size_t>::max(),
     * the function returns @c false without modifying @p result.  Leading
     * zeros are permitted.
     *
     * @param[in]  value  The string to parse.
     * @param[out] result On success, set to the parsed numeric value.
     * @return @c true if @p value consists entirely of decimal digits and
     *         the result fits in @c std::size_t; @c false otherwise.
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
     * @brief Resolve a selector string to an index within a YAML sequence.
     *
     * The @p selector is interpreted in one of two ways:
     *   - **Numeric index**: If the selector parses as a non-negative
     *     integer (via @ref numeric_index), it is used directly as the
     *     sequence index.  The index is bounds-checked against
     *     @c node.size().
     *   - **Name/target match**: Otherwise, the sequence is scanned for a
     *     map element whose @c name or @c target key matches the selector
     *     as a scalar string.  The first such element is selected.
     *
     * If neither interpretation succeeds, the process is terminated via
     * @ref invalid_path.
     *
     * @param node     The YAML sequence node to search.  Must be a valid
     *                 sequence.
     * @param selector The selector string: a decimal index or a
     *                 name/target value to match.
     * @param path     The full configuration path being traversed, used
     *                 solely for error diagnostics.
     * @return The index into @p node that @p selector resolves to.
     *
     * @warning Terminates the process via @ref invalid_path when the
     *          selector is a numeric index that is out of bounds, or when
     *          it is a name that does not match any element's @c name or
     *          @c target field.
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
     * @brief Recursively walk the parsed path segments and assign a scalar
     *        value at the target location in a YAML tree.
     *
     * This is the core recursive helper behind @ref traverse.  It consumes
     * one path segment per recursion level and dispatches based on the
     * current node's type:
     *
     *   - **Map node**: Looks up the current segment as a key.  If the key
     *     does not exist the path is considered invalid.  On the final
     *     segment the key is assigned @p value; otherwise recursion
     *     continues into the child node.
     *   - **Sequence node**: Resolves the current segment to an index via
     *     @ref sequence_index (numeric index or name/target match).  On
     *     the final segment that element is assigned @p value; otherwise
     *     recursion continues into the child node.
     *   - **Other types** (scalar, null): The path is considered invalid
     *     because traversal cannot descend into a leaf value.
     *
     * @param parts      The pre-split path segments (from @ref split_path).
     * @param position   The index of the current segment being processed.
     * @param value      The scalar string to assign at the resolved
     *                   location.
     * @param node       The YAML node at the current depth of the tree.
     * @param full_path  The original dot-separated path string, used solely
     *                   for error diagnostics.
     *
     * @warning Terminates the process via @ref invalid_path if any segment
     *          does not exist in a map, refers to an out-of-bounds or
     *          unmatched index in a sequence, or attempts to descend into a
     *          scalar or null node.
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
 * @brief Assign a scalar value to a node located by a dot-separated path.
 *
 * Splits @p path into segments (via @ref split_path) and recursively
 * walks the YAML tree rooted at @p node to find the target location,
 * then sets that location to the scalar @p value.
 *
 * Maps are traversed by key name.  Sequences are traversed by either a
 * numeric index (e.g. \c "2") or by matching the element's @c name or
 * @c target scalar field against the segment value (see
 * @ref sequence_index).
 *
 * If any segment in the path is invalid, refers to a non-existent key,
 * or addresses a non-traversable node type, the process is terminated
 * with a fatal error.
 *
 * @param path  Dot-separated path describing the location in the YAML
 *              tree (e.g. \c "build.options.debug" or
 *              \c "dependencies.libtirpc.version").  Segments that address
 *              a sequence element may use a numeric index or a bare name
 *              matched against the element's @c name or @c target field.
 * @param value The scalar string to assign at the resolved location.
 * @param node  The root YAML node to start the traversal from.
 *
 * @note This function modifies the YAML tree in place.
 * @warning Terminates the process via @ref invalid_path on any traversal
 *          failure (missing key, out-of-bounds index, unmatched name, or
 *          attempted descent into a scalar/null node).
 *
 * @see split_path
 * @see sequence_index
 * @see assign
 */
void traverse(const std::string& path, const std::string& value, YAML::Node node) {
    const std::vector<std::string> parts = split_path(path);
    assign(parts, 0, value, node, path);
}
