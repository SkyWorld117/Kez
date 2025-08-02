#include <yaml-cpp/yaml.h>
#include <iostream>
#include <filesystem>
#include <vector>
#include <unordered_map>
#include <string>

#include "../colors/colored_io.h"

#include "package_parser.h"

int main(int argc, char* argv[]) {
    // Expected arguments:
    // 1. Configuration file path
    // 2. Build mode
    // 3. Environment path

    if (argc < 4) {
        ERROR("Usage: " + std::string(argv[0]) + " <config_file> <build_mode> <env_path>");
        exit(EXIT_FAILURE);
    }

    std::filesystem::path db_path(getenv("CHEESE_DB"));

    std::string config_file = argv[1];
    std::string build_mode = argv[2];
    std::string env_path = argv[3];

    YAML::Node user_config;
    user_config = YAML::LoadFile(config_file);
    
    std::unordered_map<std::string, std::string> template_map;

    // Solve abstract linking:
    // 1. Load involved abstract packages and fetch their implementations
    // 2. Set the `use-<concrete_package_name>` attributes based on the recipe
    for (const auto& item : user_config["recipe"]["abstract_packages"]) {
        std::string abstract_pkg_name = item.first.as<std::string>();
        std::string concrete_pkg_name = user_config["recipe"]["abstract_packages"][abstract_pkg_name].as<std::string>();
        
        std::filesystem::path config_path = db_path / (abstract_pkg_name + ".yaml");
        if (!std::filesystem::exists(config_path)) {
            ERROR("Configuration file does not exist: " + config_path.string());
            exit(EXIT_FAILURE);
        }
        YAML::Node abstract_pkg_config = YAML::LoadFile(config_path.string());
        for (const auto& impl : abstract_pkg_config["cheese"]["implementations"]) {
            std::string impl_name = impl.as<std::string>();
            if (impl_name == concrete_pkg_name) {
                template_map[abstract_pkg_name + ".use-" + concrete_pkg_name] = "true";
            } else {
                template_map[abstract_pkg_name + ".use-" + impl_name] = "false";
            }
        }
    }

    // Parse configurations for each package into instructions
    std::unordered_map<std::string, std::vector<std::string>> package_instructions;

    for (const auto& item : user_config["cheese"]) {
        std::string pkg_name = item.first.as<std::string>();
        std::filesystem::path config_path = db_path / (pkg_name + ".yaml");
        if (!std::filesystem::exists(config_path)) {
            ERROR("Configuration file does not exist: " + config_path.string());
            exit(EXIT_FAILURE);
        }
        YAML::Node pkg_config = YAML::LoadFile(config_path.string());

        // Parse the package configuration
        std::vector<std::string> instructions = parse_package(
            pkg_name,
            template_map,
            user_config,
            user_config["cheese"][pkg_name],
            pkg_config,
            build_mode,
            env_path
        );

        package_instructions[pkg_name] = instructions;
    }

    // Output the instructions for each package
    for (const auto& pkg : package_instructions) {
        INFO("Instructions for package: " + pkg.first);
        for (const auto& instruction : pkg.second) {
            std::cout << "- " << instruction << std::endl;
        }
    }

    return 0;
}