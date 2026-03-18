#include <package_format_verifier/options_verifier.hpp>

bool verify_options(const YAML::Node& node) {
    if (!node.IsSequence()) {
        ERROR("Options configuration must be a sequence.");
        return false;
    }

    for (const auto& item : node) {
        if (!item.IsMap()) {
            item.Mark().line;
            ERROR("Error at '" + std::to_string(item.Mark().line) +
                  "':\nEach option entry must be a map.");
            return false;
        }

        if (!item["name"] || !item["name"].IsScalar()) {
            ERROR("Error at '" + std::to_string(item.Mark().line) +
                  "':\nEach option entry must have a 'name' field as a scalar.");
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

        if (item["enabled"]) {
            if (!item["enabled"].IsMap()) {
                ERROR("Error at '" + std::to_string(item["enabled"].Mark().line) +
                      "':\nAn 'enabled' field, if present, must be a map.");
                return false;
            }
            if (item["enabled"]["default"]) {
                if (!item["enabled"]["default"].IsScalar()) {
                    ERROR(
                        "Error at '" + std::to_string(item["enabled"]["default"].Mark().line) +
                        "':\nAn 'enabled' field's 'default' value, if present, must be a scalar.");
                    return false;
                } else if (item["enabled"]["default"].as<std::string>() != "true" &&
                           item["enabled"]["default"].as<std::string>() != "false") {
                    ERROR("Error at '" + std::to_string(item["enabled"]["default"].Mark().line) +
                          "':\nAn 'enabled' field's 'default' value, if present, must be either "
                          "'true' or 'false'.");
                    return false;
                }
            }
            if (item["enabled"]["conditions"] &&
                !verify_conditions(item["enabled"]["conditions"])) {
                ERROR("Error at '" + std::to_string(item["enabled"]["conditions"].Mark().line) +
                      "':\nInvalid conditions in 'enabled' field.");
                return false;
            }
        }

        if (item["enabled_format"] && !item["enabled_format"].IsScalar()) {
            ERROR("Error at '" + std::to_string(item["enabled_format"].Mark().line) +
                  "':\nAn 'enabled_format' field, if present, must be a scalar.");
            return false;
        }

        if (item["disabled_format"] && !item["disabled_format"].IsScalar()) {
            ERROR("Error at '" + std::to_string(item["disabled_format"].Mark().line) +
                  "':\nA 'disabled_format' field, if present, must be a scalar.");
            return false;
        }

        if (item["requires"]) {
            if (!item["requires"].IsSequence()) {
                ERROR("Error at '" + std::to_string(item["requires"].Mark().line) +
                      "':\nA 'requires' field, if present, must be a sequence.");
                return false;
            }
            for (const auto& req : item["requires"]) {
                if (!req.IsScalar()) {
                    ERROR("Error at '" + std::to_string(req.Mark().line) +
                          "':\nEach requirement in 'requires' must be a scalar.");
                    return false;
                }
            }
        }

        if (item["enabled_value"]) {
            if (!item["enabled_value"].IsMap()) {
                ERROR("Error at '" + std::to_string(item["enabled_value"].Mark().line) +
                      "':\nAn 'enabled_value' field, if present, must be a map.");
                return false;
            }
            if (item["enabled_value"]["default"] && !item["enabled_value"]["default"].IsScalar() &&
                !item["enabled_value"]["default"].IsNull()) {
                ERROR("Error at '" + std::to_string(item["enabled_value"]["default"].Mark().line) +
                      "':\nAn 'enabled_value' field's 'default' value, if present, must be a "
                      "scalar or null.");
                return false;
            }
            if (item["enabled_value"]["conditions"] &&
                !verify_conditions(item["enabled_value"]["conditions"])) {
                ERROR("Error at '" +
                      std::to_string(item["enabled_value"]["conditions"].Mark().line) +
                      "':\nInvalid conditions in 'enabled_value' field.");
                return false;
            }
            if (!item["enabled_value"]["default"] && !item["enabled_value"]["conditions"]) {
                WARNING("Warning at '" + std::to_string(item.Mark().line) +
                        "':\nAn 'enabled_value' field should have either a 'default' value or "
                        "'conditions', otherwise consider removing it.");
            }
        }

        if (item["disabled_value"]) {
            if (!item["disabled_value"].IsMap()) {
                ERROR("Error at '" + std::to_string(item["disabled_value"].Mark().line) +
                      "':\nA 'disabled_value' field, if present, must be a map.");
                return false;
            }
            if (item["disabled_value"]["default"] &&
                !item["disabled_value"]["default"].IsScalar() &&
                !item["disabled_value"]["default"].IsNull()) {
                ERROR("Error at '" + std::to_string(item["disabled_value"]["default"].Mark().line) +
                      "':\nA 'disabled_value' field's 'default' value, if present, must be a "
                      "scalar or null.");
                return false;
            }
            if (item["disabled_value"]["conditions"] &&
                !verify_conditions(item["disabled_value"]["conditions"])) {
                ERROR("Error at '" +
                      std::to_string(item["disabled_value"]["conditions"].Mark().line) +
                      "':\nInvalid conditions in 'disabled_value' field.");
                return false;
            }
            if (!item["disabled_value"]["default"] && !item["disabled_value"]["conditions"]) {
                WARNING("Warning at '" + std::to_string(item.Mark().line) +
                        "':\nA 'disabled_value' field should have either a 'default' value or "
                        "'conditions', otherwise consider removing it.");
            }
        }
    }

    return true;
}