#include <package_format_verifier/build_verifier.hpp>

bool verify_build(const YAML::Node& node) {
    if (!node.IsMap()) {
        ERROR("Build configuration must be a map.");
        return false;
    }

    if (node["preprocessing"] && !node["preprocessing"].IsScalar()) {
        ERROR("Preprocessing must be a scalar value.");
        return false;
    }

    if (node["postprocessing"] && !node["postprocessing"].IsScalar()) {
        ERROR("Postprocessing must be a scalar value.");
        return false;
    }

    if (node["configurations"] && !verify_configurations(node["configurations"])) {
        ERROR("Invalid configurations in build.");
        return false;
    }

    if (node["stages"] && !verify_stages(node["stages"])) {
        ERROR("Invalid stages in build.");
        return false;
    }

    return true;
}