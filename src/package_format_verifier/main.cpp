#include "../colors/colored_io.h"
#include "cheese_verifier.h"
#include "yaml-cpp/yaml.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        ERROR("Usage: " + std::string(argv[0]) + " <YAML file>");
        return 1;
    }

    YAML::Node config = YAML::LoadFile(argv[1]);
    if (!config.IsMap() || !config["cheese"] || config.size() != 1) {
        ERROR("Invalid YAML format. Expected a map with a single 'cheese' key.");
        return 1;
    }
    if (!verify_cheese(config["cheese"])) {
        ERROR("Cheese verification failed.");
        return 1;
    }

    SUCCESS("Cheese verification succeeded.");
    return 0;
}