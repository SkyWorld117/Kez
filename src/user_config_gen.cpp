// A user-oriented configuration generator.
// It generates a YAML configuration file based on the required packages and their dependencies.

// template:
// cheese:
//   app1:
//     compiler: string, default is "system"
//     mpi: string, entry exists only if mpi in dependencies, default is null
//     configurations: [same structure as the database package configurations, only the user-configurable variables are stored]
//   app2: ...

// Note: It is necessary to explore the `configurations` of each package as they may have different structures.

// Some configurations are modified for the ease of use:
// 1. features/packages/miscellaneous:
//   name: string
//   description: string, append `default` and `value.default` if applicable
//   use: bool
//   value: string, optional, only if `value.required` is true in the database
// 2. variables/environment:
//   name: string
//   description: string, append `default` if applicable
//   value: string or null

#include <iostream>
#include <filesystem>
#include <yaml-cpp/yaml.h>

#include <string>
#include <vector>

#include "deps_resolve.h"

YAML::Node config;

YAML::Node config_opts(const YAML::Node& db_node) {
    YAML::Node options = YAML::Node(YAML::NodeType::Sequence);
    for (const auto& opt : db_node) {
        if (!opt["user_configurable"].as<bool>()) {
            continue; // Skip non-user-configurable options
        }
        YAML::Node opt_node;
        opt_node["name"] = opt["name"].as<std::string>();
        opt_node["description"] = opt["description"].as<std::string>();
        opt_node["use"] = opt["default"].as<bool>();
        if (opt["value"]["required"].as<bool>()) {
            opt_node["value"] = opt["value"]["default"];
        }
        options.push_back(opt_node);
    }
    return options;
}

YAML::Node config_vars(const YAML::Node& db_node) {
    YAML::Node variables = YAML::Node(YAML::NodeType::Sequence);
    for (const auto& var : db_node) {
        if (!var["user_configurable"].as<bool>()) {
            continue; // Skip non-user-configurable variables
        }
        YAML::Node var_node;
        var_node["name"] = var["name"].as<std::string>();
        var_node["description"] = var["description"].as<std::string>();
        var_node["value"] = var["default"];
        variables.push_back(var_node);
    }
    return variables;
}

void config_per_pkg(const std::string& pkg_name, const YAML::Node& db_pkg_node) {
    config[pkg_name]["compiler"] = "system";
    config[pkg_name]["version"] = db_pkg_node["cheese"]["source"]["releases"][0]["version"].as<std::string>();

    // Add MPI entry if the package has MPI in its dependencies
    std::vector<std::string> dependencies = db_pkg_node["cheese"]["dependencies"].as<std::vector<std::string>>();
    if (std::find(dependencies.begin(), dependencies.end(), "mpi") != dependencies.end()) {
        config[pkg_name]["mpi"] = YAML::Node(YAML::NodeType::Null);
    }

    config[pkg_name]["configurations"] = YAML::Node(YAML::NodeType::Map);

    if (db_pkg_node["cheese"]["toolchain"].as<std::string>() == "autotools") {
        config[pkg_name]["configurations"]["optional_features"] =
            config_opts(db_pkg_node["cheese"]["build"]["configurations"]["optional_features"]);
        config[pkg_name]["configurations"]["optional_packages"] =
            config_opts(db_pkg_node["cheese"]["build"]["configurations"]["optional_packages"]);
        config[pkg_name]["configurations"]["variables"] =
            config_vars(db_pkg_node["cheese"]["build"]["configurations"]["variables"]);
        config[pkg_name]["configurations"]["environment"] =
            config_vars(db_pkg_node["cheese"]["build"]["configurations"]["environment"]);
        config[pkg_name]["configurations"]["miscellaneous"] =
            config_opts(db_pkg_node["cheese"]["build"]["configurations"]["miscellaneous"]);

        // Clean up the empty entries
        if (config[pkg_name]["configurations"]["optional_features"].size() == 0) {
            config[pkg_name]["configurations"].remove("optional_features");
        }
        if (config[pkg_name]["configurations"]["optional_packages"].size() == 0) {
            config[pkg_name]["configurations"].remove("optional_packages");
        }
        if (config[pkg_name]["configurations"]["variables"].size() == 0) {
            config[pkg_name]["configurations"].remove("variables");
        }
        if (config[pkg_name]["configurations"]["environment"].size() == 0) {
            config[pkg_name]["configurations"].remove("environment");
        }
        if (config[pkg_name]["configurations"]["miscellaneous"].size() == 0) {
            config[pkg_name]["configurations"].remove("miscellaneous");
        }

    } else if (db_pkg_node["cheese"]["toolchain"].as<std::string>() == "makefile") {
        config[pkg_name]["configurations"]["compile_time"] = YAML::Node(YAML::NodeType::Map);
        config[pkg_name]["configurations"]["compile_time"]["variables"] =
            config_vars(db_pkg_node["cheese"]["build"]["configurations"]["compile_time"]["variables"]);
        config[pkg_name]["configurations"]["compile_time"]["environment"] =
            config_vars(db_pkg_node["cheese"]["build"]["configurations"]["compile_time"]["environment"]);
        config[pkg_name]["configurations"]["compile_time"]["miscellaneous"] =
            config_opts(db_pkg_node["cheese"]["build"]["configurations"]["compile_time"]["miscellaneous"]);

        config[pkg_name]["configurations"]["install_time"] = YAML::Node(YAML::NodeType::Map);
        config[pkg_name]["configurations"]["install_time"]["variables"] =
            config_vars(db_pkg_node["cheese"]["build"]["configurations"]["install_time"]["variables"]);
        config[pkg_name]["configurations"]["install_time"]["environment"] =
            config_vars(db_pkg_node["cheese"]["build"]["configurations"]["install_time"]["environment"]);
        config[pkg_name]["configurations"]["install_time"]["miscellaneous"] =
            config_opts(db_pkg_node["cheese"]["build"]["configurations"]["install_time"]["miscellaneous"]);
        
        // Clean up the empty entries
        if (config[pkg_name]["configurations"]["compile_time"]["variables"].size() == 0) {
            config[pkg_name]["configurations"]["compile_time"].remove("variables");
        }
        if (config[pkg_name]["configurations"]["compile_time"]["environment"].size() == 0) {
            config[pkg_name]["configurations"]["compile_time"].remove("environment");
        }
        if (config[pkg_name]["configurations"]["compile_time"]["miscellaneous"].size() == 0) {
            config[pkg_name]["configurations"]["compile_time"].remove("miscellaneous");
        }
        if (config[pkg_name]["configurations"]["install_time"]["variables"].size() == 0) {
            config[pkg_name]["configurations"]["install_time"].remove("variables");
        }
        if (config[pkg_name]["configurations"]["install_time"]["environment"].size() == 0) {
            config[pkg_name]["configurations"]["install_time"].remove("environment");
        }
        if (config[pkg_name]["configurations"]["install_time"]["miscellaneous"].size() == 0) {
            config[pkg_name]["configurations"]["install_time"].remove("miscellaneous");
        }

        if (config[pkg_name]["configurations"]["compile_time"].size() == 0) {
            config[pkg_name]["configurations"].remove("compile_time");
        }
        if (config[pkg_name]["configurations"]["install_time"].size() == 0) {
            config[pkg_name]["configurations"].remove("install_time");
        }

    } else {
        std::cerr << "Unsupported toolchain: " << db_pkg_node["cheese"]["toolchain"].as<std::string>() << std::endl;
        return;
    }

    // Clean up the empty configurations
    if (config[pkg_name]["configurations"].size() == 0) {
        config[pkg_name].remove("configurations");
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <database_path>" << std::endl;
        return 1;
    }

    std::string pkg_name = argv[1];

    // std::vector<std::string> dependencies = resolve_dependencies(pkg_name); // For testing only
    std::vector<std::string> dependencies = resolve_filtered_dependencies(pkg_name);
    if (dependencies.empty()) {
        std::cerr << "No dependencies found for package: " << pkg_name << std::endl;
        return 1;
    }

    std::filesystem::path db_path(getenv("CHEESE_DB"));
    for (const auto& dep : dependencies) {
        std::filesystem::path config_file(dep + ".yaml");
        std::filesystem::path config_path = db_path / config_file;
        if (!std::filesystem::exists(config_path)) {
            std::cerr << "Configuration file does not exist: " << config_path << std::endl;
            continue;
        }
        YAML::Node db_pkg_node = YAML::LoadFile(config_path.string());
        if (!db_pkg_node["cheese"]) {
            std::cerr << "Invalid package format: " << dep << std::endl;
            continue;
        }
        config_per_pkg(dep, db_pkg_node);
    }

    // Output the generated configuration
    YAML::Emitter out;
    out << config;

    std::cout << out.c_str() << std::endl;

    return 0;
}