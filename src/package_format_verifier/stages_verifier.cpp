#include <package_format_verifier/stages_verifier.hpp>

bool verify_stages(const YAML::Node& node) {
    if (!node.IsSequence()) {
        ERROR("Stages must be a sequence.");
        return false;
    }

    for (const auto& stage : node) {
        if (!stage.IsMap()) {
            ERROR("Each stage must be a map.");
            return false;
        }

        if (!stage["target"] || (!stage["target"].IsScalar() && !stage["target"].IsNull())) {
            ERROR("Error at '" + std::to_string(stage.Mark().line) +
                  "':\n"
                  "Each stage must have a 'target' key with a scalar or null value.");
            return false;
        }

        if (stage["multithreaded"]) {
            if (!stage["multithreaded"].IsScalar()) {
                ERROR("Error at '" + std::to_string(stage.Mark().line) +
                      "':\n"
                      "The 'multithreaded' key must be a scalar value.");
                return false;
            }
            if (stage["multithreaded"].as<std::string>() != "true" &&
                stage["multithreaded"].as<std::string>() != "false") {
                ERROR("Error at '" + std::to_string(stage.Mark().line) +
                      "':\n"
                      "The 'multithreaded' key must be either 'true' or 'false'.");
                return false;
            }
        }

        if (stage["configurations"] && !verify_configurations(stage["configurations"])) {
            return false;
        }
    }

    return true;
}