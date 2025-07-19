#include "configurations_verifier.h"

bool verify_configurations(const YAML::Node& node) {
    if (!node.IsMap()) {
        ERROR("Configurations must be a map.");
        return false;
    }

    if (node["environment"] && !verify_environment(node["environment"])) {
        return false;
    }

    if (node["options"] && !verify_options(node["options"])) {
        return false;
    }

    if (!node["environment"] && !node["options"]) {
        WARNING("Configurations should have either 'environment' or 'options', otherwise consider removing this section.");
    }
    return true;
}