#include "environment_verifier.h"

bool verify_environment(const YAML::Node& node) {
    if (!node.IsSequence()) {
        ERROR("Error: Environment configuration must be a sequence.");
        return false;
    }

    for (const auto& item : node) {
        if (!item.IsMap()) {
            item.Mark().line;
            ERROR("Error at '" + std::to_string(item.Mark().line) +
                  "':\nEach environment entry must be a map.");
            return false;
        }

        if (!item["name"] || !item["name"].IsScalar()) {
            ERROR("Error at '" + std::to_string(item.Mark().line) +
                  "':\nEach environment entry must have a 'name' field as a scalar.");
            return false;
        }

        if (item["description"] && !item["description"].IsScalar()) {
            ERROR("Error at '" + std::to_string(item["description"].Mark().line) +
                  "':\nA 'description' field, if present, must be a scalar.");
            return false;
        }

        if (item["user_configurable"]) {
            if (!item["user_configurable"].IsScalar()) {
                ERROR("Error at '" + std::to_string(item["user_configurable"].Mark().line) +
                      "':\nA 'user_configurable' field, if present, must be a scalar.");
                return false;
            } else if (item["user_configurable"].as<std::string>() != "true" &&
                       item["user_configurable"].as<std::string>() != "false") {
                ERROR("Error at '" + std::to_string(item["user_configurable"].Mark().line) +
                      "':\nA 'user_configurable' field, if present, must be either 'true' or "
                      "'false'.");
                return false;
            }
        }

        if (item["default"] && !item["default"].IsScalar()) {
            ERROR("Error at '" + std::to_string(item["default"].Mark().line) +
                  "':\nA 'default' field, if present, must be a scalar.");
            return false;
        }

        if (item["conditions"] && !verify_conditions(item["conditions"])) {
            ERROR("Error at '" + std::to_string(item["conditions"].Mark().line) +
                  "':\nInvalid conditions in environment entry.");
            return false;
        }

        // Some additional checks for logic
        if (item["user_configurable"] && !item["user_configurable"].as<bool>() &&
            !item["default"] && !item["conditions"]) {
            WARNING("Warning at '" + std::to_string(item.Mark().line) +
                    "':\nA non-user-configurable environment entry should have either a 'default' "
                    "value or 'conditions', otherwise consider removing it.");
        }
    }

    return true;
}