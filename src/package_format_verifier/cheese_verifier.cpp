#include "cheese_verifier.h"

bool verify_cheese(const YAML::Node& node) {
    if (!node.IsMap()) {
        ERROR("Cheese definition must be a map.");
        return false;
    }

    if (!node["name"] || !node["name"].IsScalar()) {
        ERROR("Cheese must have a 'name' field of type scalar.");
        return false;
    }

    if (node["description"] && !node["description"].IsScalar()) {
        ERROR("Cheese 'description' must be a scalar if provided.");
        return false;
    }

    if (!node["type"] || !node["type"].IsScalar()) {
        ERROR("Cheese must have a 'type' field of type scalar.");
        return false;
    }

    if (node["toolchain"] && !node["toolchain"].IsScalar()) {
        ERROR("Cheese 'toolchain' must be a scalar if provided.");
        return false;
    }

    if (node["source"] && !verify_source(node["source"])) {
        ERROR("Cheese must have a valid 'source' field.");
        return false;
    }

    if (node["dependencies"] && !verify_dependencies(node["dependencies"])) {
        ERROR("Cheese must have a valid 'dependencies' field.");
        return false;
    }

    if (node["build"] && !verify_build(node["build"])) {
        ERROR("Cheese must have a valid 'build' field.");
        return false;
    }

    if (node["properties"] &&
        !verify_properties(node["properties"], node["type"].as<std::string>())) {
        ERROR("Cheese must have a valid 'properties' field.");
        return false;
    }

    if (node["type"].as<std::string>() == "abstract") {
        if (!node["implementations"] || !verify_implementations(node["implementations"])) {
            ERROR("Abstract cheese must have a valid 'implementations' field.");
            return false;
        }
    }

    if (!verify_templates(node)) {
        return false;
    }

    return true;
}