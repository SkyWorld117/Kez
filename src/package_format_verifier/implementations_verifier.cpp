#include "implementations_verifier.h"

bool verify_implementations(const YAML::Node& node) {
    if (!node.IsSequence()) {
        ERROR("Implementations must be a sequence.");
        return false;
    }

    for (const auto& item : node) {
        if (!item.IsScalar()) {
            ERROR("Each implementation must be a scalar.");
            return false;
        }
    }

    return true;
}