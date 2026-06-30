#include <filesystem>
#include <user_config_generator/user_config_generator.hpp>
#include <utils/bash_utils.hpp>

void config_per_pkg(YAML::Node& config, const YAML::Node& db_pkg_node,
                    const std::vector<std::string>& all_dependencies,
                    const std::vector<std::string>& target_packages) {
    std::string pkg_name = db_pkg_node["cheese"]["name"].as<std::string>();

    config["cheese"][pkg_name] = YAML::Node(YAML::NodeType::Map);
    if (db_pkg_node["cheese"]["description"]) {
        config["cheese"][pkg_name]["description"] =
            db_pkg_node["cheese"]["description"].as<std::string>();
    }
    if (db_pkg_node["cheese"]["source"]) {
        // Default to the latest release
        config["cheese"][pkg_name]["version"] =
            db_pkg_node["cheese"]["source"]["releases"][0]["version"].as<std::string>();
    }
    if (db_pkg_node["cheese"]["type"].as<std::string>() != "vendor" &&
        db_pkg_node["cheese"]["type"].as<std::string>() != "external") {
        config["cheese"][pkg_name]["compiler"] = global_config::get_default_compiler();
    }

    std::filesystem::path patches_dir(get_env_var("FROMAGER_HOME") + "/patches/" + pkg_name);
    if (std::filesystem::exists(patches_dir) && std::filesystem::is_directory(patches_dir)) {
        config["cheese"][pkg_name]["patches"] = YAML::Node(YAML::NodeType::Sequence);
        for (const auto& entry : std::filesystem::directory_iterator(patches_dir)) {
            if (entry.is_regular_file()) {
                YAML::Node patch_node = YAML::Node(YAML::NodeType::Map);
                patch_node["name"] = entry.path().filename().string();
                patch_node["enabled"] = false;  // Default to disabled, user can enable in the config
                config["cheese"][pkg_name]["patches"].push_back(patch_node);
            }
        }
    }

    // MPI needs to be handled separately
    bool pkg_is_target = (db_pkg_node["cheese"]["type"].as<std::string>() != "mpi" &&
                          db_pkg_node["cheese"]["type"].as<std::string>() != "compiler") ||
                         (std::find(target_packages.begin(), target_packages.end(), pkg_name) !=
                          target_packages.end());
    if (db_pkg_node["cheese"]["build"] && pkg_is_target) {
        if (db_pkg_node["cheese"]["build"]["configurations"]) {
            if (db_pkg_node["cheese"]["build"]["configurations"]) {
                YAML::Node filtered_config = filtered_configurations(
                    db_pkg_node["cheese"]["build"]["configurations"], all_dependencies,
                    config["recipe"]["abstract_packages"]);
                if (!filtered_config.IsNull() && !(filtered_config.size() == 0)) {
                    if (!config["cheese"][pkg_name]["build"]) {
                        config["cheese"][pkg_name]["build"] = YAML::Node(YAML::NodeType::Map);
                    }
                    config["cheese"][pkg_name]["build"]["configurations"] = filtered_config;
                }
            }
        }
        if (db_pkg_node["cheese"]["build"]["stages"]) {
            YAML::Node filtered_stages_node =
                filtered_stages(db_pkg_node["cheese"]["build"]["stages"], all_dependencies,
                                config["recipe"]["abstract_packages"]);
            if (!filtered_stages_node.IsNull() && !(filtered_stages_node.size() == 0)) {
                if (!config["cheese"][pkg_name]["build"]) {
                    config["cheese"][pkg_name]["build"] = YAML::Node(YAML::NodeType::Map);
                }
                config["cheese"][pkg_name]["build"]["stages"] = filtered_stages_node;
            }
        }
    }
}

YAML::Node gen_user_config(const std::vector<std::string>& pkg_name, bool interactive) {
    std::pair<std::pair<std::vector<std::string>, std::vector<std::string>>,
              std::unordered_map<std::string, std::string>>
        result                                = resolve_dependencies(pkg_name, interactive);
    std::vector<std::string> all_dependencies = result.first.first;
    std::vector<std::string> dependencies     = result.first.second;
    std::unordered_map<std::string, std::string> abstract_packages = result.second;
    if (dependencies.empty()) {
        // This makes sense because at least the package itself should be included
        std::string pkg_names_str = "";
        for (const std::string& pkg : pkg_name) {
            pkg_names_str += pkg + " ";
        }
        pkg_names_str = pkg_names_str.substr(0, pkg_names_str.size() - 1);  // Remove trailing space
        ERROR("No dependencies found for packages: " + pkg_names_str);
        exit(EXIT_FAILURE);
    }

    YAML::Node config = YAML::Node(YAML::NodeType::Map);
    config["cheese"]  = YAML::Node(YAML::NodeType::Map);

    // Additional section to store abstract package selections
    config["recipe"]                      = YAML::Node(YAML::NodeType::Map);
    config["recipe"]["abstract_packages"] = YAML::Node(YAML::NodeType::Map);
    for (const auto& pair : abstract_packages) {
        config["recipe"]["abstract_packages"][pair.first] = pair.second;
    }
    // Add a list of all dependencies
    config["recipe"]["dependencies"] = YAML::Node(YAML::NodeType::Sequence);
    for (const auto& dep : all_dependencies) {
        config["recipe"]["dependencies"].push_back(dep);
    }
    // Add a list of target packages (the ones explicitly requested by the user)
    config["recipe"]["targets"] = YAML::Node(YAML::NodeType::Sequence);
    for (const auto& target : pkg_name) {
        config["recipe"]["targets"].push_back(target);
    }

    std::filesystem::path db_path(get_env_var("FROMAGER_DB"));
    for (const auto& dep : dependencies) {
        YAML::Node db_pkg_node = get_db_config(dep);
        config_per_pkg(config, db_pkg_node, all_dependencies, pkg_name);
    }

    return config;
}