#include <yaml-cpp/yaml.h>
#include <iostream>
#include <fstream>
#include <filesystem>

#include "../colors/colored_io.h"
#include "parser.h"

int main(int argc, char* argv[]) {
    // Expected arguments:
    // 1. Configuration file path
    // 2. Build mode
    // 3. Environment path

    if (argc < 4) {
        ERROR("Usage: " + std::string(argv[0]) + " <config_file> <build_mode> <env_path>");
        exit(EXIT_FAILURE);
    }

    std::string config_file = argv[1];
    std::string build_mode = argv[2];
    std::string env_path = argv[3];

    YAML::Node user_config;
    user_config = YAML::LoadFile(config_file);

    YAML::Node instructions_yaml = parse(user_config, build_mode, env_path);

    // Create the file to `env_path/.tmp/ins.yaml`
    YAML::Emitter out;
    out << instructions_yaml;

    std::filesystem::path tmp_path = std::filesystem::path(env_path) / ".tmp";
    std::filesystem::create_directories(tmp_path);
    std::ofstream ofs((tmp_path / "ins.yaml").string());
    if (!ofs) {
        ERROR("Failed to create instruction file");
        exit(EXIT_FAILURE);
    }
    ofs << out.c_str();
    ofs.close();

    SUCCESS("Instructions written to: " + (tmp_path / "ins.yaml").string());

    return 0;
}