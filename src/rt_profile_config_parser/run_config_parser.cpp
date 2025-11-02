#include "run_config_parser.h"

std::string multilevel_fetch(
    const YAML::Node& factory_config,
    const YAML::Node& cellar_config,
    const YAML::Node& profile_config,
    const std::string& key
) {
    // Check profile_config
    if (profile_config[key]) {
        return profile_config[key].as<std::string>();
    }

    // Check cellar_config
    if (cellar_config[key]) {
        return cellar_config[key].as<std::string>();
    }

    // Check factory_config
    if (factory_config[key]) {
        return factory_config[key].as<std::string>();
    }

    return "";
}

std::pair<YAML::Node, std::string> parse_run_config(const YAML::Node& factory_config, const YAML::Node& cellar_config, const YAML::Node& profile_config) {
    if (!profile_config.IsMap()) {
        ERROR("Profile configuration should be a map");
        exit(EXIT_FAILURE);
    }

    if (!profile_config["name"] || !profile_config["name"].IsScalar()) {
        ERROR("Profile configuration missing 'name' key or 'name' is not a scalar");
        exit(EXIT_FAILURE);
    }

    if (profile_config["sibling"]) {
        if (!profile_config["sibling"].IsScalar()) {
            ERROR("'sibling' key should be a scalar");
            exit(EXIT_FAILURE);
        }

        std::string sibling_name = profile_config["sibling"].as<std::string>();
        for (const auto& sibling_profile : cellar_config["profiles"]) {
            if (sibling_profile["name"].as<std::string>() == sibling_name) {
                return parse_run_config(factory_config, cellar_config, sibling_profile);
            }
        }
    }

    std::vector<std::string> run_instructions;

    // Copy inputs
    std::string inputs = multilevel_fetch(factory_config, cellar_config, profile_config, "inputs");
    if (inputs.empty()) {
        WARNING("'inputs' key is not defined.");
    } else {
        run_instructions.push_back("cp -r " + inputs + "/* ./");
    }

    // Prerun commands
    std::string prerun = multilevel_fetch(factory_config, cellar_config, profile_config, "prerun");
    if (!prerun.empty()) {
        run_instructions.push_back(prerun);
    }

    // Main command
    std::string target = multilevel_fetch(factory_config, cellar_config, profile_config, "target");
    if (target.empty()) {
        ERROR("'target' key is not defined.");
        exit(EXIT_FAILURE);
    }

    // Resource manager wrapping
    std::string launcher = multilevel_fetch(factory_config, cellar_config, profile_config, "launcher");
    if (launcher.empty()) {
        launcher = "none";
    }
    std::string launcher_opts = multilevel_fetch(factory_config, cellar_config, profile_config, "launcher_opts");
    
    std::string scheduler = multilevel_fetch(factory_config, cellar_config, profile_config, "scheduler");
    if (scheduler.empty()) {
        scheduler = "none";
    }
    std::string scheduler_opts = multilevel_fetch(factory_config, cellar_config, profile_config, "scheduler_opts");

    std::string num_nodes = profile_config["num_nodes"] ? profile_config["num_nodes"].as<std::string>() : "1";
    std::string num_procs_per_node = profile_config["num_procs_per_node"] ? profile_config["num_procs_per_node"].as<std::string>() : "1";
    std::string cores_per_proc = profile_config["cores_per_proc"] ? profile_config["cores_per_proc"].as<std::string>() : "1";
    std::string omp_num_threads = profile_config["omp_num_threads"] ? profile_config["omp_num_threads"].as<std::string>() : cores_per_proc;
    std::string gpus_per_proc = profile_config["gpus_per_proc"] ? profile_config["gpus_per_proc"].as<std::string>() : "0";

    std::string wrapped_command = wrap_with_resource_manager(
        target,
        launcher,
        launcher_opts,
        scheduler,
        scheduler_opts,
        num_nodes,
        num_procs_per_node,
        cores_per_proc,
        omp_num_threads,
        gpus_per_proc
    );
    run_instructions.push_back(wrapped_command);

    // Postrun commands
    std::string postrun = multilevel_fetch(factory_config, cellar_config, profile_config, "postrun");
    if (!postrun.empty()) {
        run_instructions.push_back(postrun);
    }

    // Prepare YAML node
    YAML::Node run_instructions_yaml(YAML::NodeType::Sequence);
    for (const auto& instruction : run_instructions) {
        run_instructions_yaml.push_back(instruction);
    }

    std::string summary_regex = multilevel_fetch(factory_config, cellar_config, profile_config, "summary_regex");
    return std::pair(run_instructions_yaml, summary_regex);
}