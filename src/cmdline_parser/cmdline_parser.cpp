#include <cmdline_parser/cmdline_parser.hpp>
#include <filesystem>
#include <unordered_map>
#include <utils/file_utils.hpp>

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
    DEBUG("Starting node before applying config options:\n" + YAML::Dump(starting_node));
    for (const auto& [key, value] : config_map) {
        traverse(key, value, starting_node);
    }
    DEBUG("User config after applying config options:\n" + YAML::Dump(user_config));
    // Parse instructions
    YAML::Node instructions = parse(user_config, "release", cellar_path);

    // Store instructions in the cellar
    std::filesystem::path ins_path = std::filesystem::path(cellar_path) / ".tmp" / "ins.yaml";
    write_yaml(instructions, ins_path.string(), "Instructions written to: " + ins_path.string());
}
