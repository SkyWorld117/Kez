// A user-oriented configuration generator.
// It generates a YAML configuration file based on the required packages and their dependencies.

#include <yaml-cpp/yaml.h>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>

#include "colors/colored_io.h"
#include "dependency_resolver/resolve_dependencies.h"

YAML::Node config = YAML::Node(YAML::NodeType::Map);


YAML::Node filtered_environment(const YAML::Node& env_node) {
    YAML::Node env = YAML::Node(YAML::NodeType::Sequence);
    for (const auto& var : env_node) {
        if (var["user_configurable"] && var["user_configurable"].as<bool>()) {
            YAML::Node tmp_var = YAML::Node(YAML::NodeType::Map);
            for (const auto& key : var) {
                if (key.first.as<std::string>() == "default") {
                    tmp_var["value"] = key.second;
                } else if (key.first.as<std::string>() != "user_configurable" && 
                           key.first.as<std::string>() != "condition") {
                    tmp_var[key.first] = key.second;
                }
            }
            env.push_back(tmp_var);
        }
    }
    return env;
}

YAML::Node filtered_options(const YAML::Node& opts_node) {
    YAML::Node opts = YAML::Node(YAML::NodeType::Sequence);
    for (const auto& opt : opts_node) {
        if (opt["user_configurable"] && opt["user_configurable"].as<bool>()) {
            YAML::Node tmp_opt = YAML::Node(YAML::NodeType::Map);

            // Early abort if the option is not enabled (insufficient conditions)
            if (opt["requires"]) {
                std::vector<std::string> all_dependencies = config["recipe"]["dependencies"].as<std::vector<std::string>>();
                bool all_deps_present = true;
                for (const auto& dep : opt["requires"]) {
                    if (std::find(all_dependencies.begin(), all_dependencies.end(), dep.as<std::string>()) == all_dependencies.end()) {
                        all_deps_present = false;
                        break;
                    }
                }
                if (!all_deps_present) {
                    continue; // Skip this option if not all required dependencies are present
                }
            }

            tmp_opt["name"] = opt["name"];

            if (opt["description"]) {
                tmp_opt["description"] = opt["description"].as<std::string>();
            }

            if (opt["enabled"]) {
                if (opt["enabled"]["default"]) {
                    tmp_opt["enabled"] = opt["enabled"]["default"];
                }
                // Otherwise probably determined via condition, do not touch it then.
            } else {
                tmp_opt["enabled"] = true; // Default to enabled if not specified
            }

            if (opt["enabled_value"]) {
                if (opt["enabled_value"]["default"]) {
                    tmp_opt["enabled_value"] = opt["enabled_value"]["default"];
                }
            } else {
                tmp_opt["enabled_value"] = YAML::Node(YAML::NodeType::Null); // Default to null if not specified
            }

            if (opt["disabled_format"]) {
                if (opt["disabled_value"]) {
                    if (opt["disabled_value"]["default"]) {
                        tmp_opt["disabled_value"] = opt["disabled_value"]["default"];
                    }
                } else {
                    tmp_opt["disabled_value"] = YAML::Node(YAML::NodeType::Null); // Default to null if not specified
                }
            }

            if (opt["requires"]) {
                tmp_opt["requires"] = opt["requires"];
            }

            opts.push_back(tmp_opt);
        }
    }
    return opts;
}

YAML::Node filtered_configurations(const YAML::Node& config_node) {
    YAML::Node configs = YAML::Node(YAML::NodeType::Map);
    if (config_node["environment"]) {
        YAML::Node env = filtered_environment(config_node["environment"]);
        if (!env.IsNull() && !(env.size() == 0)) {
            configs["environment"] = env;
        }
    }
    if (config_node["options"]) {
        YAML::Node opts = filtered_options(config_node["options"]);
        if (!opts.IsNull() && !(opts.size() == 0)) {
            configs["options"] = opts;
        }
    }
    return configs;
}

YAML::Node filtered_stages(const YAML::Node& stages_node) {
    YAML::Node stages = YAML::Node(YAML::NodeType::Sequence);
    for (const auto& stage : stages_node) {
        if (stage["configurations"]) {
            YAML::Node filtered_config = filtered_configurations(stage["configurations"]);
            if (!filtered_config.IsNull() && !(filtered_config.size() == 0)) {
                YAML::Node tmp_stage = YAML::Node(YAML::NodeType::Map);
                tmp_stage["target"] = stage["target"];
                tmp_stage["configurations"] = filtered_config;
                stages.push_back(tmp_stage);
            }
        }
    }
    return stages;
}

void config_per_pkg(const YAML::Node& db_pkg_node) {
    std::string pkg_name = db_pkg_node["cheese"]["name"].as<std::string>();

    config["cheese"][pkg_name] = YAML::Node(YAML::NodeType::Map);
    if (db_pkg_node["cheese"]["description"]) {
        config["cheese"][pkg_name]["description"] = db_pkg_node["cheese"]["description"].as<std::string>();
    }
    if (db_pkg_node["cheese"]["source"]) {
        // Default to the latest release
        config["cheese"][pkg_name]["version"] = db_pkg_node["cheese"]["source"]["releases"][0]["version"];
    }
    if (db_pkg_node["cheese"]["type"].as<std::string>() != "vendor" && db_pkg_node["cheese"]["type"].as<std::string>() != "external") {
        config["cheese"][pkg_name]["compiler"] = "system"; // default to system compiler
    }
    // MPI needs to be handled separately
    std::vector<std::string> all_dependencies = config["recipe"]["dependencies"].as<std::vector<std::string>>();
    bool pkg_is_target = (db_pkg_node["cheese"]["type"].as<std::string>() != "mpi" &&
                         db_pkg_node["cheese"]["type"].as<std::string>() != "compiler") ||
                         all_dependencies[0] == pkg_name;
    if (db_pkg_node["cheese"]["build"] && pkg_is_target) {
        if (db_pkg_node["cheese"]["build"]["configurations"]) {
            if (db_pkg_node["cheese"]["build"]["configurations"]) {
                YAML::Node filtered_config = filtered_configurations(db_pkg_node["cheese"]["build"]["configurations"]);
                if (!filtered_config.IsNull() && !(filtered_config.size() == 0)) {
                    if (!config["cheese"][pkg_name]["build"]) {
                        config["cheese"][pkg_name]["build"] = YAML::Node(YAML::NodeType::Map);
                    }
                    config["cheese"][pkg_name]["build"]["configurations"] = filtered_config;
                }
            }
        }
        if (db_pkg_node["cheese"]["build"]["stages"]) {
            YAML::Node filtered_stages_node = filtered_stages(db_pkg_node["cheese"]["build"]["stages"]);
            if (!filtered_stages_node.IsNull() && !(filtered_stages_node.size() == 0)) {
                if (!config["cheese"][pkg_name]["build"]) {
                    config["cheese"][pkg_name]["build"] = YAML::Node(YAML::NodeType::Map);
                }
                config["cheese"][pkg_name]["build"]["stages"] = filtered_stages_node;
            }
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        ERROR("Usage: " + std::string(argv[0]) + " <package_name> [<output_file>]");
        exit(EXIT_FAILURE);
    }

    std::string pkg_name = argv[1];

    std::pair<std::pair<std::vector<std::string>, std::vector<std::string>>, std::unordered_map<std::string, std::string>> result = resolve_dependencies(pkg_name);
    std::vector<std::string> all_dependencies = result.first.first;
    std::vector<std::string> dependencies = result.first.second;
    std::unordered_map<std::string, std::string> abstract_packages = result.second;
    if (dependencies.empty()) {
        ERROR("No dependencies found for package: " + pkg_name);
        exit(EXIT_FAILURE);
    }

    config["cheese"] = YAML::Node(YAML::NodeType::Map);

    // Additional section to store abstract package selections
    config["recipe"] = YAML::Node(YAML::NodeType::Map);
    config["recipe"]["abstract_packages"] = YAML::Node(YAML::NodeType::Map);
    for (const auto& pair : abstract_packages) {
        config["recipe"]["abstract_packages"][pair.first] = pair.second;
    }
    // Add a list of all dependencies
    config["recipe"]["dependencies"] = YAML::Node(YAML::NodeType::Sequence);
    for (const auto& dep : all_dependencies) {
        config["recipe"]["dependencies"].push_back(dep);
    }

    std::filesystem::path db_path(getenv("FROMAGER_DB"));
    for (const auto& dep : dependencies) {
        std::filesystem::path config_file(dep + ".yaml");
        std::filesystem::path config_path = db_path / config_file;
        if (!std::filesystem::exists(config_path)) {
            ERROR("Configuration file does not exist: " + config_path.string());
            exit(EXIT_FAILURE);
        }
        YAML::Node db_pkg_node = YAML::LoadFile(config_path.string());
        config_per_pkg(db_pkg_node);
    }

    // Output the generated configuration
    YAML::Emitter out;
    out << config;

    std::cout << out.c_str() << std::endl;

    // If an output file is specified, write the configuration to it
    if (argc > 2) {
        std::string output_file = argv[2];
        std::ofstream ofs(output_file);
        if (!ofs) {
            ERROR("Could not open output file: " + output_file);
            exit(EXIT_FAILURE);
        }
        ofs << out.c_str();
        ofs.close();
        SUCCESS("Configuration written to: " + output_file);
    } else {
        SUCCESS("Configuration output to stdout.");
    }

    return 0;
}