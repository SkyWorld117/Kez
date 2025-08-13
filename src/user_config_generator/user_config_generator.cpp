#include "user_config_generator.h"

YAML::Node config = YAML::Node(YAML::NodeType::Map);

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
                YAML::Node filtered_config = filtered_configurations(db_pkg_node["cheese"]["build"]["configurations"], all_dependencies);
                if (!filtered_config.IsNull() && !(filtered_config.size() == 0)) {
                    if (!config["cheese"][pkg_name]["build"]) {
                        config["cheese"][pkg_name]["build"] = YAML::Node(YAML::NodeType::Map);
                    }
                    config["cheese"][pkg_name]["build"]["configurations"] = filtered_config;
                }
            }
        }
        if (db_pkg_node["cheese"]["build"]["stages"]) {
            YAML::Node filtered_stages_node = filtered_stages(db_pkg_node["cheese"]["build"]["stages"], all_dependencies);
            if (!filtered_stages_node.IsNull() && !(filtered_stages_node.size() == 0)) {
                if (!config["cheese"][pkg_name]["build"]) {
                    config["cheese"][pkg_name]["build"] = YAML::Node(YAML::NodeType::Map);
                }
                config["cheese"][pkg_name]["build"]["stages"] = filtered_stages_node;
            }
        }
    }
}

YAML::Node gen_user_config(const std::string& pkg_name, bool interactive) {
    std::pair<std::pair<std::vector<std::string>, std::vector<std::string>>, std::unordered_map<std::string, std::string>> result = resolve_dependencies(pkg_name, interactive);
    std::vector<std::string> all_dependencies = result.first.first;
    std::vector<std::string> dependencies = result.first.second;
    std::unordered_map<std::string, std::string> abstract_packages = result.second;
    if (dependencies.empty()) {
        // This makes sense because at least the package itself should be included
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

    return config;
}