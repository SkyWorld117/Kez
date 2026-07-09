/**
 * @file  yaml_utils.cpp
 * @brief Utility functions for loading and querying YAML nodes with
 *        descriptive error reporting.
 *
 * All functions that dereference YAML content terminate the program on
 * malformed input via ERROR() + exit(EXIT_FAILURE) rather than throwing.
 */

#include <cstdlib>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <utils/colored_io.hpp>
#include <utils/yaml_utils.hpp>

namespace {
    /**
     * @brief Cache of previously loaded YAML files, keyed by absolute,
     *        lexically-normalised path.
     *
     * Used by cached_yaml_load() to avoid re-parsing the same file.
     * The cache is process-local and grows monotonically; entries are
     * never evicted.
     */
    std::unordered_map<std::string, YAML::Node> yaml_cache;
}  // namespace

/**
 * @brief Load a YAML file from disk, caching the result.
 *
 * The path is resolved to an absolute, lexically-normalised string and used
 * as the cache key.  On cache hit the previously-parsed YAML::Node is
 * returned immediately; on cache miss the file is parsed with
 * YAML::LoadFile() and inserted into the cache.
 *
 * @param path  Filesystem path to the YAML file (relative or absolute).
 * @return The root YAML::Node of the parsed file.
 *
 * @warning YAML::LoadFile() will throw a YAML::Exception if the file does
 *          not exist or contains invalid syntax.  This function does not
 *          catch that exception — the program terminates via
 *          std::terminate if an unhandled exception reaches main().
 */
YAML::Node cached_yaml_load(const std::filesystem::path& path) {
    const std::string key = std::filesystem::absolute(path).lexically_normal().string();
    const auto cached     = yaml_cache.find(key);
    if (cached != yaml_cache.end()) {
        return cached->second;
    }
    YAML::Node node = YAML::LoadFile(key);
    yaml_cache.emplace(key, node);
    return node;
}

/**
 * @brief Check whether a YAML node is a map containing a given key.
 *
 * Returns false if the node is not a map at all, or if the key is present
 * but explicitly undefined (YAML null / ~).  This is a safe query that
 * never terminates the program.
 *
 * @param node  The YAML node to inspect.
 * @param key   The key to look for within @p node.
 * @return true if @p node is a map and @p node[key] is defined;
 *         false otherwise.
 */
bool yaml_has(const YAML::Node& node, const std::string& key) {
    return node.IsMap() && node[key].IsDefined();
}

/**
 * @brief Extract the scalar string value from a YAML node.
 *
 * @param node         The YAML node that is expected to hold a scalar.
 * @param description  Human-readable description of the expected content
 *                     (e.g. "package version"), used in the error message.
 * @return The scalar value as a std::string.
 *
 * @note Terminates the program via ERROR() + exit(EXIT_FAILURE) if
 *       @p node is not a scalar (e.g. it is a map, sequence, or null).
 */
std::string yaml_scalar(const YAML::Node& node, const std::string& description) {
    if (!node.IsScalar()) {
        ERROR("Invalid YAML: " + description + " must be a scalar");
        exit(EXIT_FAILURE);
    }
    return node.Scalar();
}

/**
 * @brief Extract a boolean value from a YAML scalar node.
 *
 * Accepts the string literals "true", "True", "TRUE" for true and
 * "false", "False", "FALSE" for false.  Any other scalar content —
 * including non-scalar nodes — causes termination with an error.
 *
 * @param node         The YAML node that is expected to hold a boolean.
 * @param description  Human-readable description of the expected content
 *                     (e.g. "enable_mpi"), used in the error message.
 * @return true or false as determined by the scalar text.
 *
 * @note Terminates the program via ERROR() + exit(EXIT_FAILURE) if
 *       @p node is not a scalar or if the scalar text is not a recognised
 *       boolean literal.
 */
bool yaml_boolean(const YAML::Node& node, const std::string& description) {
    const std::string value = yaml_scalar(node, description);
    if (value == "true" || value == "True" || value == "TRUE") {
        return true;
    }
    if (value == "false" || value == "False" || value == "FALSE") {
        return false;
    }
    ERROR("Invalid YAML: " + description + " must be true or false");
    exit(EXIT_FAILURE);
}
