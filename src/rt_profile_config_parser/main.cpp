#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <fstream>
#include <iostream>

#include "../colors/colored_io.h"
#include "factory_parser.h"

int main(int argc, char* argv[]) {
    // Expected arguments:
    // 1. Path to config.yaml in a factory directory
    // 2. Path to the factory directory

    if (argc < 3) {
        ERROR("Usage: " + std::string(argv[0]) + " <config_file> <factory_dir>");
        exit(EXIT_FAILURE);
    }

    std::string config_file = argv[1];

    YAML::Node factory_config;
    factory_config = YAML::LoadFile(config_file);

    if (!factory_config["factory"]) {
        ERROR("Invalid factory configuration file: missing 'factory' key");
        exit(EXIT_FAILURE);
    }
    YAML::Node instructions_yaml = parse_factory_config(factory_config["factory"]);

    // Create the file to `factory_dir/ins.yaml`
    YAML::Emitter out;
    out << instructions_yaml;

    std::filesystem::path factory_path = std::filesystem::path(argv[2]);
    std::filesystem::path output_path  = factory_path / "ins.yaml";
    std::ofstream         ofs(output_path.string());
    if (!ofs) {
        ERROR("Failed to create instruction file");
        exit(EXIT_FAILURE);
    }
    ofs << out.c_str();
    ofs.close();

    SUCCESS("Instructions written to: " + output_path.string());

    return 0;
}