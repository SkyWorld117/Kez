#include <cstdlib>
#include <string>
#include <utils/colored_io.hpp>
#include <utils/yaml_utils.hpp>

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
