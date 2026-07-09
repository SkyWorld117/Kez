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
 * @brief Loads a YAML file from disk with in-process caching, keyed by
 *        absolute normalized path.
 *
 * On first invocation for a given path the file is read and parsed via
 * YAML::LoadFile; subsequent calls for the same path return the cached
 * YAML::Node.  The cache is never evicted during the lifetime of the
 * process.
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
 * @brief Checks whether a YAML map node contains a defined value for the
 *        given key.
 *
 * Returns false if the node is not a map or if the key is missing or
 * explicitly null.  Does not validate that the value is of any particular
 * type.
 */
bool yaml_has(const YAML::Node& node, const std::string& key) {
    return node.IsMap() && node[key].IsDefined();
}

/**
 * @brief Validates that a YAML node is a scalar and returns its string
 *        value.
 *
 * Terminates the program via ERROR() if the node is not a scalar.  The
 * @p description argument is used in the error message to identify which
 * field was expected to be a scalar.
 */
std::string yaml_scalar(const YAML::Node& node, const std::string& description) {
    if (!node.IsScalar()) {
        ERROR("Invalid YAML: " + description + " must be a scalar");
        exit(EXIT_FAILURE);
    }
    return node.Scalar();
}

/**
 * @brief Parses a YAML scalar as a boolean, accepting true/True/TRUE and
 *        false/False/FALSE.
 *
 * Delegates to yaml_scalar for type validation and terminates the program
 * if the string does not match any recognised boolean literal.  The
 * @p description argument identifies the field in error messages.
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
