#include <cmdline_parser/cmdline_parser.hpp>
#include <unordered_map>

#include "yaml-cpp/node/node.h"

void parse_cmdline(const std::string& file, const std::string& cellar_path,
                   const std::vector<std::string>& config_options) {
    YAML::Node user_config = YAML::LoadFile(file);
    parse_cmdline(user_config, cellar_path, config_options);
}

void parse_cmdline(const std::vector<std::string>& target, const std::string& cellar_path,
                   const std::vector<std::string>& config_options) {
    YAML::Node user_config = gen_user_config(target, false);
    parse_cmdline(user_config, cellar_path, config_options);
}

void parse_cmdline(YAML::Node user_config, const std::string& cellar_path,
                   const std::vector<std::string>& config_options) {
    // Parse config options
    std::unordered_map<std::string, std::string> config_map;
    for (const auto& option : config_options) {
        size_t pos = option.find('=');
        if (pos != std::string::npos) {
            std::string key   = option.substr(0, pos);
            std::string value = option.substr(pos + 1);
            config_map[key]   = value;
        } else {
            ERROR("Invalid configuration format: " + option);
            exit(EXIT_FAILURE);
        }
    }

    // Modify user config based on command line options
    YAML::Node starting_node = user_config["cheese"];
    for (const auto& [key, value] : config_map) {
        traverse(key, value, starting_node);
    }

    // Parse instructions
    YAML::Node instructions = parse(user_config, "release", cellar_path);

    // Store instructions in the cellar
    YAML::Emitter out;
    out << instructions;

    std::filesystem::path tmp_path = std::filesystem::path(cellar_path) / ".tmp";
    std::filesystem::create_directories(tmp_path);
    std::ofstream ofs((tmp_path / "ins.yaml").string());
    if (!ofs) {
        ERROR("Failed to create instruction file");
        exit(EXIT_FAILURE);
    }
    ofs << out.c_str();
    ofs << std::endl;
    ofs.close();

    SUCCESS("Instructions written to: " + (tmp_path / "ins.yaml").string());
}