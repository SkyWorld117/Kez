#include <parser/context.hpp>
#include <parser/fromager_parser.hpp>
#include <parser/parser.hpp>

YAML::Node parse(YAML::Node& user_config, const std::string& build_mode,
                 const std::string& env_path) {
    // Parser Context
    ParserContext context;
    context.user_config.reset(user_config);
    context.build_mode = build_mode;
    context.env_path   = env_path;

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
                context.template_map[abstract_pkg_name + ".use-" + concrete_pkg_name] = "true";
            } else {
                context.template_map[abstract_pkg_name + ".use-" + impl_name] = "false";
            }
        }
    }

    // Parse configurations for each package into instructions
    std::unordered_map<std::string, std::vector<std::string>> package_instructions;

    for (const auto& item : user_config["cheese"]) {
        DEBUG("Processing package: " + item.first.as<std::string>());
        std::string pkg_name = item.first.as<std::string>();

        // Skip if already processed
        if (package_instructions.find(pkg_name) != package_instructions.end()) {
            continue;
        }

        DEBUG("Loading configuration for package: " + pkg_name);
        YAML::Node pkg_config = get_db_config(pkg_name);
        DEBUG("Configuration for package '" + pkg_name + "':\n" + YAML::Dump(pkg_config));

        // Update context for the current package
        context.package_name = pkg_name;
        context.user_config_pkg.reset(user_config["cheese"][pkg_name]);
        context.pkg_config.reset(pkg_config);
        context.user_config_context.reset(user_config["cheese"][pkg_name]);

        // Parse the package configuration
        DEBUG("Parsing package configuration for: " + pkg_name);
        std::vector<std::string> instructions = parse_package(context);

        package_instructions[pkg_name] = instructions;
    }

    for (const auto& pkg : user_config["cheese"]) {
        DEBUG("Second pass for property parsing in package: " + pkg.first.as<std::string>());
        std::string pkg_name = pkg.first.as<std::string>();
        if (package_instructions.find(pkg_name) == package_instructions.end()) {
            continue;  // Skip if no instructions found
        }
        for (auto& instruction : package_instructions[pkg_name]) {
            // Parse properties in the instruction
            instruction = parse_properties_in_scalar(instruction, context);
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
