#include <parser/parser.hpp>

#include "parser/scalar_parser.hpp"

YAML::Node parse(YAML::Node& user_config, const std::string& build_mode,
                 const std::string& env_path) {
    std::unordered_map<std::string, std::string> template_map;

    // Solve abstract linking:
    // 1. Load involved abstract packages and fetch their implementations
    // 2. Set the `use-<concrete_package_name>` attributes based on the recipe
    for (const auto& item : user_config["recipe"]["abstract_packages"]) {
        std::string abstract_pkg_name = item.first.as<std::string>();
        std::string concrete_pkg_name =
            user_config["recipe"]["abstract_packages"][abstract_pkg_name].as<std::string>();

        YAML::Node abstract_pkg_config = get_db_config(abstract_pkg_name);
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
        DEBUG("Processing package: " + item.first.as<std::string>());
        if (build_mode == "debug") {
            INFO("Processing package: " + item.first.as<std::string>());
        }
        std::string pkg_name = item.first.as<std::string>();

        // Skip if already processed
        if (package_instructions.find(pkg_name) != package_instructions.end()) {
            continue;
        }

        if (build_mode == "debug") {
            INFO("Loading configuration for package: " + pkg_name);
        }
        YAML::Node pkg_config = get_db_config(pkg_name);
        DEBUG("Configuration for package '" + pkg_name + "':\n" + YAML::Dump(pkg_config));

        // Parse the package configuration
        if (build_mode == "debug") {
            INFO("Parsing package configuration for: " + pkg_name);
        }
        std::vector<std::string> instructions =
            parse_package(pkg_name, template_map, user_config, user_config["cheese"][pkg_name],
                          user_config["cheese"][pkg_name], pkg_config, build_mode, env_path);

        package_instructions[pkg_name] = instructions;
    }

    for (const auto& pkg : user_config["cheese"]) {
        if (build_mode == "debug") {
            INFO("Second pass for property parsing in package: " + pkg.first.as<std::string>());
        }
        std::string pkg_name = pkg.first.as<std::string>();
        if (package_instructions.find(pkg_name) == package_instructions.end()) {
            continue;  // Skip if no instructions found
        }
        for (auto& instruction : package_instructions[pkg_name]) {
            // Parse properties in the instruction
            instruction =
                parse_properties_in_scalar(instruction, template_map, user_config,
                                           user_config["cheese"][pkg_name], build_mode, env_path);
            instruction = parse_scalar(instruction, template_map, user_config,
                                       user_config["cheese"][pkg_name], build_mode, env_path);
            filter(instruction);  // Filter the instruction
        }
    }

    // Output the instructions for each package
    for (const auto& pkg : user_config["cheese"]) {
        INFO("Instructions for package: " + pkg.first.as<std::string>());
        for (const auto& instruction : package_instructions[pkg.first.as<std::string>()]) {
            std::cout << "- " << instruction << std::endl;
        }
    }

    // Convert package_instructions to YAML format
    YAML::Node instructions_yaml = YAML::Node(YAML::NodeType::Sequence);
    std::vector<std::string> all_dependencies =
        user_config["recipe"]["dependencies"].as<std::vector<std::string>>();
    std::reverse(all_dependencies.begin(), all_dependencies.end());
    for (const std::string& pkg_name : all_dependencies) {
        if (package_instructions.find(pkg_name) != package_instructions.end() &&
            !package_instructions[pkg_name].empty()) {
            YAML::Node pkg_node(YAML::NodeType::Map);
            pkg_node["package"]      = pkg_name;
            pkg_node["instructions"] = package_instructions[pkg_name];
            instructions_yaml.push_back(pkg_node);
        }
    }

    return instructions_yaml;
}
