#include "traverse.h"

void traverse(const std::string& path, const std::string& value, YAML::Node& node) {
    if (path.empty()) {
        ERROR("Path cannot be empty");
        return;
    }

    size_t pos                 = path.find('.');
    std::string key            = (pos == std::string::npos) ? path : path.substr(0, pos);
    std::string remaining_path = (pos == std::string::npos) ? "" : path.substr(pos + 1);

    if (!node[key]) {
        ERROR("Invalid configuration path: " + path);
        exit(EXIT_FAILURE);
    }

    YAML::Node current_node = node[key];

    if (remaining_path.empty()) {
        current_node = value;
    } else {
        traverse(remaining_path, value, current_node);
    }
}