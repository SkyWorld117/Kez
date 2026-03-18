#include <package_format_verifier/dependencies_verifier.hpp>

bool verify_dependencies(const YAML::Node& node) {
    if (!node.IsSequence()) {
        ERROR("Dependencies must be a sequence.");
        return false;
    }

    for (const auto& dep : node) {
        if (!dep.IsScalar()) {
            ERROR("Each dependency must be a scalar value.");
            return false;
        }
    }
    return true;
}