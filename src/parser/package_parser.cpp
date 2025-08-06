#include "package_parser.h"

std::vector<std::string> parse_package(
    const std::string& package_name,
    std::unordered_map<std::string, std::string>& template_map,
    const YAML::Node& user_config,
    const YAML::Node& user_config_pkg,
    const YAML::Node& user_config_context,
    const YAML::Node& pkg_config,
    const std::string& build_mode,
    const std::string& env_path
) {
    std::vector<std::string> instructions;

    if (!pkg_config["cheese"]["build"]) {
        return instructions; // No build instructions available
    }

    if ((pkg_config["cheese"]["type"].as<std::string>() == "compiler" ||
        pkg_config["cheese"]["type"].as<std::string>() == "mpi") &&
        !user_config_context["build"]) {
        return instructions; // No build context available for compilers or MPI
    }

    // TODO: Implement downloading etc.

    // Preprocessing
    if (build_mode == "debug") {
        INFO("- Preprocessing for package: " + package_name);
    }
    if (pkg_config["cheese"]["build"]["preprocessing"]) {
        std::string preprocessing = parse_scalar(
            pkg_config["cheese"]["build"]["preprocessing"].as<std::string>(),
            template_map,
            user_config,
            user_config_pkg,
            build_mode,
            env_path
        );
        instructions.push_back(preprocessing);
    }

    // Compiling

    // Global configurations
    if (build_mode == "debug") {
        INFO("- Global configurations for package: " + package_name);
    }
    if (pkg_config["cheese"]["build"]["configurations"]) {
        YAML::Node configurations = pkg_config["cheese"]["build"]["configurations"];
        YAML::Node user_config_context_config;
        if (user_config_context["build"]["configurations"]) {
            user_config_context_config = user_config_context["build"]["configurations"];
        }
        std::pair<std::vector<std::string>, std::string> parsed_configurations = parse_configuration(
            configurations,
            template_map,
            user_config,
            user_config_pkg,
            user_config_context_config,
            pkg_config,
            build_mode,
            env_path
        );
        std::vector<std::string> env_config = parsed_configurations.first;
        std::string opts_config = parsed_configurations.second;
        if (pkg_config["cheese"]["toolchain"]) {
            if (pkg_config["cheese"]["toolchain"].as<std::string>() == "autotools") {
                if (!opts_config.empty()) {
                    opts_config = "./configure " + opts_config;
                } else {
                    opts_config = "./configure";
                }
            } else if (pkg_config["cheese"]["toolchain"].as<std::string>() == "cmake") {
                if (!opts_config.empty()) {
                    opts_config = "cmake ../ " + opts_config;
                } else {
                    opts_config = "cmake ../";
                }
            }
            // Ignore the others for now
        } else {
            opts_config = "";
        }
        if (opts_config.empty()) {
            for (const auto& env : env_config) {
                instructions.push_back("export " + env);
            }
        } else {
            std::string command = opts_config;
            std::reverse(env_config.begin(), env_config.end());
            for (const auto& env : env_config) {
                command = env + " " + command;
            }
            instructions.push_back(command);
        }
    }

    // Stages
    if (build_mode == "debug") {
        INFO("- Stages for package: " + package_name);
    }
    std::string threads = getenv("FROMAGER_NPROC");
    if (pkg_config["cheese"]["build"]["stages"]) {
        YAML::Node stages = pkg_config["cheese"]["build"]["stages"];

        for (const auto& stage : stages) {
            std::string stage_target;
            if (stage["target"].IsScalar()) {
                stage_target = parse_scalar(
                    stage["target"].as<std::string>(),
                    template_map,
                    user_config,
                    user_config_pkg,
                    build_mode,
                    env_path
                );
            } else if (stage["target"].IsNull()) {
                stage_target = ""; // Default to empty string if not specified
            } else {
                ERROR("Invalid target type in stage: " + stage["target"].Type());
                exit(EXIT_FAILURE);
            }

            bool multithreaded;
            if (stage["multithreaded"] && stage["multithreaded"].IsScalar()) {
                multithreaded = stage["multithreaded"].as<bool>();
            } else {
                multithreaded = true; // Default to true if not specified
            }

            if (!stage_target.empty()) {
                stage_target = " " + stage_target;
            }

            if (multithreaded && !threads.empty()) {
                stage_target = "make -j" + threads + stage_target;
            } else {
                stage_target = "make" + stage_target;
            }

            if (stage["configurations"]) {
                YAML::Node stage_configurations = stage["configurations"];
                YAML::Node user_config_context_config;
                if (user_config_context["build"]["stages"]) {
                    for (const auto& user_stage : user_config_context["build"]["stages"]) {
                        if (user_stage["target"].as<std::string>() == stage["target"].as<std::string>()) {
                            user_config_context_config = user_stage["configurations"];
                            break;
                        }
                    }
                }
                std::pair<std::vector<std::string>, std::string> parsed_stage_configurations = parse_configuration(
                    stage_configurations,
                    template_map,
                    user_config,
                    user_config_pkg,
                    user_config_context_config,
                    pkg_config,
                    build_mode,
                    env_path
                );
                std::vector<std::string> stage_env_config = parsed_stage_configurations.first;
                std::reverse(stage_env_config.begin(), stage_env_config.end());
                for (const auto& env : stage_env_config) {
                    stage_target = env + " " + stage_target;
                }
                if (!parsed_stage_configurations.second.empty()) {
                    stage_target += " " + parsed_stage_configurations.second;
                }
            }

            instructions.push_back(stage_target);
        }
    }

    // Postprocessing
    if (build_mode == "debug") {
        INFO("- Postprocessing for package: " + package_name);
    }
    if (pkg_config["cheese"]["build"]["postprocessing"]) {
        std::string postprocessing = parse_scalar(
            pkg_config["cheese"]["build"]["postprocessing"].as<std::string>(),
            template_map,
            user_config,
            user_config_pkg,
            build_mode,
            env_path
        );
        instructions.push_back(postprocessing);
    }

    return instructions;
}