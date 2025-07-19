// A user-oriented configuration generator.
// It generates a YAML configuration file based on the required packages and their dependencies.

#include <yaml-cpp/yaml.h>
#include <iostream>
#include <filesystem>
#include <string>
#include <vector>

#include "colors/colored_io.h"
#include "deps_resolve.h"

YAML::Node config = YAML::Node(YAML::NodeType::Map);


YAML::Node filtered_environment(const YAML::Node& env_node) {
    YAML::Node env = YAML::Node(YAML::NodeType::Sequence);
    for (const auto& var : env_node) {
        if (var["user_configurable"] && var["user_configurable"].as<bool>()) {
            YAML::Node tmp_var = YAML::Node(YAML::NodeType::Map);
            for (const auto& key : var) {
                if (key.first.as<std::string>() != "user_configurable") {
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
            for (const auto& key : opt) {
                if (key.first.as<std::string>() != "user_configurable") {
                    tmp_opt[key.first] = key.second;
                }
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
    } else {
        config["cheese"][pkg_name]["version"] = YAML::Node(YAML::NodeType::Null);
    }
    config["cheese"][pkg_name]["compiler"] = "system"; // default to system compiler
    if (db_pkg_node["cheese"]["implementations"]) {
        // Choose the first implementation as default
        config["cheese"][pkg_name]["implementation"] = db_pkg_node["cheese"]["implementations"][0].as<std::string>();
    }
    config["cheese"][pkg_name]["build"] = YAML::Node(YAML::NodeType::Map);
    if (db_pkg_node["cheese"]["build"]["configurations"]) {
        if (db_pkg_node["cheese"]["build"]["configurations"]) {
            YAML::Node filtered_config = filtered_configurations(db_pkg_node["cheese"]["build"]["configurations"]);
            if (!filtered_config.IsNull() && !(filtered_config.size() == 0)) {
                config["cheese"][pkg_name]["build"]["configurations"] = filtered_config;
            }
        }
    }
    if (db_pkg_node["cheese"]["build"]["stages"]) {
        YAML::Node filtered_stages_node = filtered_stages(db_pkg_node["cheese"]["build"]["stages"]);
        if (!filtered_stages_node.IsNull() && !(filtered_stages_node.size() == 0)) {
            config["cheese"][pkg_name]["build"]["stages"] = filtered_stages_node;
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        ERROR("Usage: " + std::string(argv[0]) + " <package_name>");
        return 1;
    }

    std::string pkg_name = argv[1];

    std::vector<std::string> dependencies = resolve_dependencies(pkg_name); // For testing only
    // std::vector<std::string> dependencies = resolve_filtered_dependencies(pkg_name);
    if (dependencies.empty()) {
        ERROR("No dependencies found for package: " + pkg_name);
        return 1;
    }

    config["cheese"] = YAML::Node(YAML::NodeType::Map);

    std::filesystem::path db_path(getenv("CHEESE_DB"));
    for (const auto& dep : dependencies) {
        std::filesystem::path config_file(dep + ".yaml");
        std::filesystem::path config_path = db_path / config_file;
        if (!std::filesystem::exists(config_path)) {
            ERROR("Configuration file does not exist: " + config_path.string());
            continue;
        }
        YAML::Node db_pkg_node = YAML::LoadFile(config_path.string());
        config_per_pkg(db_pkg_node);
    }

    // Output the generated configuration
    YAML::Emitter out;
    out << config;

    std::cout << out.c_str() << std::endl;

    return 0;
}