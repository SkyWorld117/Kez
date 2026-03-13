#include "package_parser.h"

std::vector<std::string> parse_package(const std::string& package_name,
                                       std::unordered_map<std::string, std::string>& template_map,
                                       const YAML::Node& user_config,
                                       const YAML::Node& user_config_pkg,
                                       const YAML::Node& user_config_context,
                                       const YAML::Node& pkg_config, const std::string& build_mode,
                                       const std::string& env_path) {
    std::vector<std::string> instructions;

    if (!pkg_config["cheese"]["build"]) {
        return instructions;  // No build instructions available
    }

    if ((pkg_config["cheese"]["type"].as<std::string>() == "compiler" ||
         pkg_config["cheese"]["type"].as<std::string>() == "mpi") &&
        !user_config_context["build"]) {
        return instructions;  // No build context available for compilers or MPI
    }

    // Build vendor packages only if they are not already built
    if (pkg_config["cheese"]["type"].as<std::string>() == "vendor" &&
        user_config_context["version"]) {
        std::string version               = user_config_context["version"].as<std::string>();
        std::filesystem::path vendor_path = std::filesystem::path(getenv("FROMAGER_ENV")) /
                                            "vendors" / (package_name + "-" + version);
        if (std::filesystem::exists(vendor_path)) {
            return instructions;  // Skip building if the vendor package already exists
        } else {
            instructions.push_back("mkdir -p " + vendor_path.string());
        }
    }

    // Download
    if (pkg_config["cheese"]["source"] && user_config_pkg["version"]) {
        std::string version = user_config_pkg["version"].as<std::string>();
        bool found          = false;
        for (const auto& release : pkg_config["cheese"]["source"]["releases"]) {
            std::string release_version = release["version"].as<std::string>();
            if (release_version == version) {
                std::string source_type = pkg_config["cheese"]["source"]["type"].as<std::string>();

                if (source_type == "tarball") {
                    // Case 1: Tarball
                    // Tarball has `url` entry
                    std::string url = release["url"].as<std::string>();
                    if (url.find(".tar") == std::string::npos) {
                        ERROR("Invalid tarball URL for package: " + package_name);
                        exit(EXIT_FAILURE);
                    }
                    std::string ext = url.substr(url.find(".tar"));
                    instructions.push_back("wget --quiet --show-progress --no-check-certificate "
                                           "--output-document=source" +
                                           ext + " " + url);
                    if (ext == ".tar.gz") {
                        instructions.push_back("tar -xzf source" + ext);
                        instructions.push_back(
                            "mv $(tar -tzf source" + ext +
                            " | head -1 | sed 's|^\\./||' | cut -f1 -d'/') source");
                    } else if (ext == ".tar.xz") {
                        instructions.push_back("tar -xf source" + ext);
                        instructions.push_back(
                            "mv $(tar -tf source" + ext +
                            " | head -1 | sed 's|^\\./||' | cut -f1 -d'/') source");
                    } else if (ext == ".tar") {
                        instructions.push_back("tar -xf source" + ext);
                        instructions.push_back(
                            "mv $(tar -tf source" + ext +
                            " | head -1 | sed 's|^\\./||' | cut -f1 -d'/') source");
                    } else {
                        ERROR("Unimplemented tarball format: " + ext);
                        exit(EXIT_FAILURE);
                    }
                    instructions.push_back("rm source" + ext);
                    instructions.push_back("cd source");
                } else if (source_type == "git") {
                    // Case 2: Git
                    // Git has `tag` entry
                    std::string git_url = pkg_config["cheese"]["source"]["url"].as<std::string>();
                    std::string git_tag = release["tag"].as<std::string>();
                    instructions.push_back("git clone " + git_url + " source");
                    instructions.push_back("cd source");
                    instructions.push_back("git checkout " + git_tag);
                } else if (source_type == "script" && release["url"]) {
                    // Case 3: Script
                    // Script may or may not have `url` entry
                    // When not present, we assume the developer calls a script from `bin` at the preprocessing or postprocessing stage (no need to handle this case)
                    // Else, we download the script
                    std::string script_url = release["url"].as<std::string>();
                    instructions.push_back(
                        "wget --quiet --show-progress --output-document=source " + script_url);
                } else {
                    // Handle unknown source types
                    ERROR("Unknown source type for package: " + package_name);
                    exit(EXIT_FAILURE);
                }

                found = true;
                break;
            }
        }
        if (!found) {
            ERROR("Version " + version +
                  " not found in source releases for package: " + package_name);
            exit(EXIT_FAILURE);
        }
    }

    // Preprocessing
    if (build_mode == "debug") {
        INFO("- Preprocessing for package: " + package_name);
    }
    if (pkg_config["cheese"]["build"]["preprocessing"]) {
        std::string preprocessing =
            parse_scalar(pkg_config["cheese"]["build"]["preprocessing"].as<std::string>(),
                         template_map, user_config, user_config_pkg, build_mode, env_path);
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
        if (user_config_context["build"] && user_config_context["build"]["configurations"]) {
            user_config_context_config = user_config_context["build"]["configurations"];
        }
        std::pair<std::vector<std::string>, std::string> parsed_configurations =
            parse_configuration(configurations, template_map, user_config, user_config_pkg,
                                user_config_context_config, pkg_config, build_mode, env_path);
        std::vector<std::string> env_config = parsed_configurations.first;
        std::string opts_config             = parsed_configurations.second;
        std::string cmd;
        if (pkg_config["cheese"]["build"]["configurations"]["command"]) {
            cmd = pkg_config["cheese"]["build"]["configurations"]["command"].as<std::string>();
        } else {
            if (pkg_config["cheese"]["toolchain"] &&
                pkg_config["cheese"]["toolchain"].as<std::string>() == "autotools") {
                cmd = "./configure";
            } else if (pkg_config["cheese"]["toolchain"] &&
                       pkg_config["cheese"]["toolchain"].as<std::string>() == "cmake") {
                instructions.push_back("mkdir -p build && cd build");
                cmd = "cmake ../";
            } else {
                // Ignore the others for now and set to empty string
                cmd = "";
            }
        }
        if (!opts_config.empty() && !cmd.empty()) {
            opts_config = cmd + " " + opts_config;
        } else {
            opts_config = cmd;
        }
        // if (opts_config.empty()) {
        //     for (const auto& env : env_config) {
        //         instructions.push_back("export " + env);
        //     }
        // } else {
        //     std::string command = opts_config;
        //     std::reverse(env_config.begin(), env_config.end());
        //     for (const auto& env : env_config) {
        //         command = env + " " + command;
        //     }
        //     instructions.push_back(command);
        // }
        for (const auto& env : env_config) {
            instructions.push_back("export " + env);
        }
        if (!opts_config.empty()) {
            instructions.push_back(opts_config);
        }
        for (const auto& env : env_config) {
            instructions.push_back("unset " + env.substr(0, env.find('=')));
        }
    }

    // Stages
    if (build_mode == "debug") {
        INFO("- Stages for package: " + package_name);
    }
    std::string threads = getenv("FROMAGER_NPROC");

    std::string toolchain = "";
    if (pkg_config["cheese"]["toolchain"])
        toolchain = pkg_config["cheese"]["toolchain"].as<std::string>();

    if (pkg_config["cheese"]["build"]["stages"]) {
        YAML::Node stages = pkg_config["cheese"]["build"]["stages"];

        for (const auto& stage : stages) {
            std::string stage_target;
            if (stage["target"].IsScalar()) {
                stage_target = parse_scalar(stage["target"].as<std::string>(), template_map,
                                            user_config, user_config_pkg, build_mode, env_path);
            } else if (stage["target"].IsNull()) {
                stage_target = "";  // Default to empty string if not specified
            } else {
                ERROR("Invalid target type in stage: " + stage["target"].Type());
                exit(EXIT_FAILURE);
            }

            bool multithreaded;
            if (stage["multithreaded"] && stage["multithreaded"].IsScalar()) {
                multithreaded = stage["multithreaded"].as<bool>();
            } else {
                multithreaded = true;  // Default to true if not specified
            }

            std::string stage_cmd;

            if (toolchain == "cmake") {
                stage_cmd = "cmake --build .";

                if (multithreaded && !threads.empty()) {
                    stage_cmd += " --parallel " + threads;
                }

                if (!stage_target.empty()) {
                    stage_cmd += " --target " + stage_target;
                }
            } else {
                stage_cmd = "make";

                if (multithreaded && !threads.empty()) {
                    stage_cmd += " -j" + threads;
                }

                if (!stage_target.empty()) {
                    stage_cmd += " " + stage_target;
                }
            }

            if (stage["configurations"]) {
                YAML::Node stage_configurations = stage["configurations"];
                YAML::Node user_config_context_config;
                if (user_config_context["build"] && user_config_context["build"]["stages"]) {
                    for (const auto& user_stage : user_config_context["build"]["stages"]) {
                        if (user_stage["target"].as<std::string>() ==
                            stage["target"].as<std::string>()) {
                            user_config_context_config = user_stage["configurations"];
                            break;
                        }
                    }
                }
                std::pair<std::vector<std::string>, std::string> parsed_stage_configurations =
                    parse_configuration(stage_configurations, template_map, user_config,
                                        user_config_pkg, user_config_context_config, pkg_config,
                                        build_mode, env_path);
                std::vector<std::string> stage_env_config = parsed_stage_configurations.first;
                std::reverse(stage_env_config.begin(), stage_env_config.end());
                for (const auto& env : stage_env_config) {
                    stage_cmd = env + " " + stage_cmd;
                }
                if (!parsed_stage_configurations.second.empty()) {
                    stage_cmd += " " + parsed_stage_configurations.second;
                }
            }

            instructions.push_back(stage_cmd);
        }
    }

    // Postprocessing
    if (build_mode == "debug") {
        INFO("- Postprocessing for package: " + package_name);
    }
    if (pkg_config["cheese"]["build"]["postprocessing"]) {
        std::string postprocessing =
            parse_scalar(pkg_config["cheese"]["build"]["postprocessing"].as<std::string>(),
                         template_map, user_config, user_config_pkg, build_mode, env_path);
        instructions.push_back(postprocessing);
    }

    return instructions;
}
