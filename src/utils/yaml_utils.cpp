#include <cstdlib>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <utils/colored_io.hpp>
#include <utils/yaml_utils.hpp>

namespace {
    std::unordered_map<std::string, YAML::Node> yaml_cache;
}  // namespace

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
