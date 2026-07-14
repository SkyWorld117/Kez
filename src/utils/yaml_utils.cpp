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

YAML::Node load_yaml_file(const std::filesystem::path& path) {
    try {
        return YAML::LoadFile(path.string());
    } catch (const YAML::BadFile&) {
        ERROR("Cannot open YAML file: " + path.string());
        exit(EXIT_FAILURE);
    } catch (const YAML::Exception& err) {
        ERROR("Failed to parse YAML file '" + path.string() + "':\n" + err.what());
        exit(EXIT_FAILURE);
    }
}

YAML::Node cached_yaml_load(const std::filesystem::path& path) {
    const std::string key = std::filesystem::absolute(path).lexically_normal().string();
    const auto cached     = yaml_cache.find(key);
    if (cached != yaml_cache.end()) {
        return cached->second;
    }
    YAML::Node node = load_yaml_file(path);
    yaml_cache.emplace(key, node);
    return node;
}

bool yaml_has(const YAML::Node& node, const std::string& key) {
    return node.IsMap() && node[key].IsDefined();
}

std::string yaml_scalar(const YAML::Node& node, const std::string& description) {
    if (!node.IsScalar()) {
        ERROR("Invalid YAML: " + description + " must be a scalar");
        exit(EXIT_FAILURE);
    }
    return node.Scalar();
}

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
