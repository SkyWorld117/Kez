#include <yaml-cpp/yaml.h>

#include <colors/colored_io.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <rt_dependency_resolver/dependents.hpp>
#include <rt_dependency_resolver/unbuilt_dependencies.hpp>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    // Expected arguments:
    // 1. User configuration file path
    // 2. Cellar path
    // 3. Target package

    // Goal: Return a yaml list of filtered instructions that:
    // 1. If dependency of the target package but not installed yet, install it
    // 2. If target package is a dependency of another package, reinstall that package regardless of whether it's installed or not

    if (argc < 4) {
        ERROR("Usage: " + std::string(argv[0]) +
              " <user_config_file> <cellar_path> <target_package>");
        exit(EXIT_FAILURE);
    }

    std::string user_config_file = argv[1];
    std::string cellar_path      = argv[2];
    std::string ins_file         = std::filesystem::path(cellar_path) / ".tmp/ins.yaml";
    std::string state_file       = std::filesystem::path(cellar_path) / "state.yaml";
    std::string target_package   = argv[3];

    std::vector<std::string> packages_to_build;

    YAML::Node user_config;
    user_config = YAML::LoadFile(user_config_file);

    YAML::Node ins_yaml;
    ins_yaml = YAML::LoadFile(ins_file);

    // `state.yaml` may not exist if no packages are installed yet
    std::vector<std::string> installed_packages;
    if (!std::filesystem::exists(state_file)) {
        installed_packages = {};
    } else {
        YAML::Node state_yaml;
        state_yaml         = YAML::LoadFile(state_file);
        installed_packages = state_yaml["cheese"].as<std::vector<std::string>>();
    }

    // Get unbuilt dependencies
    std::vector<std::string> unbuilt_dependencies =
        get_unbuilt_dependencies(user_config, ins_yaml, installed_packages, target_package);
    packages_to_build.insert(packages_to_build.end(), unbuilt_dependencies.begin(),
                             unbuilt_dependencies.end());
    // Get dependents
    std::vector<std::string> dependents =
        get_dependents(user_config, ins_yaml, installed_packages, target_package);
    packages_to_build.insert(packages_to_build.end(), dependents.begin(), dependents.end());

    // Remove duplicates from packages_to_build
    std::sort(packages_to_build.begin(), packages_to_build.end());
    packages_to_build.erase(std::unique(packages_to_build.begin(), packages_to_build.end()),
                            packages_to_build.end());

    // Add target_package itself if not included
    if (std::find(packages_to_build.begin(), packages_to_build.end(), target_package) ==
        packages_to_build.end()) {
        packages_to_build.push_back(target_package);
    }

    // Filter the instructions in ins_yaml based on packages_to_build
    YAML::Node filtered_instructions = YAML::Node(YAML::NodeType::Sequence);
    for (const auto& pkg_instruction : ins_yaml) {
        std::string pkg_name = pkg_instruction["package"].as<std::string>();
        if (std::find(packages_to_build.begin(), packages_to_build.end(), pkg_name) !=
            packages_to_build.end()) {
            filtered_instructions.push_back(pkg_instruction);
        }
    }

    // Output the filtered instructions as yaml
    YAML::Emitter out;
    out << filtered_instructions;

    std::filesystem::path filtered_ins_file =
        std::filesystem::path(cellar_path) / ".tmp/filtered_ins.yaml";
    std::ofstream ofs(filtered_ins_file.string());
    if (!ofs) {
        ERROR("Failed to create instruction file");
        exit(EXIT_FAILURE);
    }
    ofs << out.c_str();
    ofs.close();

    SUCCESS("Filtered instructions written to: " + filtered_ins_file.string());
    return 0;
}