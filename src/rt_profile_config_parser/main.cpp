#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <rt_profile_config_parser/factory_parser.hpp>
#include <utils/colored_io.hpp>
#include <utils/file_utils.hpp>

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
    std::filesystem::path factory_path = std::filesystem::path(argv[2]);
    std::filesystem::path output_path = factory_path / "ins.yaml";
    write_yaml(instructions_yaml, output_path.string(), "Instructions written to: " + output_path.string());

    return 0;
}