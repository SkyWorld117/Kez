#include <filesystem>
#include <global_config.hpp>
#include <parser/package_parser.hpp>
#include <parser/source_parser.hpp>

std::vector<std::string> parse_package(ParserContext& context) {
    std::vector<std::string> instructions;

    // Unpack context
    const std::string& package_name      = context.package_name;
    const YAML::Node user_config_pkg     = context.user_config_pkg;
    const YAML::Node user_config_context = context.user_config_context;
    const YAML::Node pkg_config          = context.pkg_config;

    if (!pkg_config["cheese"]["build"]) {
        return instructions;  // No build instructions available
    }

    if ((pkg_config["cheese"]["type"].as<std::string>() == "compiler" ||
         pkg_config["cheese"]["type"].as<std::string>() == "mpi") &&
        !user_config_context["build"]) {
        return instructions;  // No build context available for compilers or MPI
    }

    // Build vendor packages only if they are not already built
    if (pkg_config["cheese"]["type"].as<std::string>() == "vendor" && user_config_pkg["version"]) {
        std::string version = user_config_pkg["version"].as<std::string>();
        std::filesystem::path vendor_path =
            std::filesystem::path(global_config::get_path("vendors")) /
            (package_name + "-" + version);
        if (std::filesystem::exists(vendor_path)) {
            return instructions;  // Skip building if the vendor package already exists
        } else {
            instructions.push_back("mkdir -p " + vendor_path.string());
        }
    }

    // Download
    if (pkg_config["cheese"]["source"] && user_config_pkg["version"]) {
        std::string version     = user_config_pkg["version"].as<std::string>();
        std::string source_type = pkg_config["cheese"]["source"]["type"].as<std::string>();

        bool found = false;
        for (const auto& release : pkg_config["cheese"]["source"]["releases"]) {
            std::string release_version = release["version"].as<std::string>();
            if (release_version == version) {
                download_source(pkg_config, release, package_name, source_type, instructions);
                found = true;
                break;
            }
        }
        if (!found) {
            WARNING("Version " + version +
                    " not found in source releases for package: " + package_name);
            WARNING("Assuming the version is a path to a local source directory.");
            // version: myname@/path/to/source
            size_t at_pos = version.find("@");
            if (at_pos == std::string::npos) {
                ERROR("Invalid version format. Expected 'version@source_path' for local sources.");
                exit(EXIT_FAILURE);
            }
            std::filesystem::path source_path = version.substr(at_pos + 1);
            if (!std::filesystem::exists(source_path)) {
                ERROR("Source path does not exist: " + source_path.string());
                exit(EXIT_FAILURE);
            }
            version = version.substr(0, at_pos);  // Extract the version part before the colon
            instructions.push_back("cp -r " + source_path.string() + " source");
            instructions.push_back("cd source");
        }
    }

    // Preprocessing
    DEBUG("- Preprocessing for package: " + package_name);
    if (pkg_config["cheese"]["build"]["preprocessing"]) {
        std::string preprocessing =
            parse_scalar(pkg_config["cheese"]["build"]["preprocessing"].as<std::string>(), context);
        instructions.push_back(preprocessing);
    }

    // Compiling
    std::string toolchain;
    if (pkg_config["cheese"]["toolchain"]) {
        toolchain = pkg_config["cheese"]["toolchain"].as<std::string>();
    }

    // Global configurations
    DEBUG("- Global configurations for package: " + package_name);
    if (pkg_config["cheese"]["build"]["configurations"]) {
        YAML::Node configurations = pkg_config["cheese"]["build"]["configurations"];
        YAML::Node user_config_context_config;
        if (user_config_context["build"] && user_config_context["build"]["configurations"]) {
            user_config_context_config = user_config_context["build"]["configurations"];
        }
        context.user_config_context.reset(user_config_context_config);

        std::string command;
        if (configurations["command"]) {
            command = configurations["command"].as<std::string>();
        } else {
            if (toolchain == "autotools") {
                command = "./configure";
            } else if (toolchain == "cmake") {
                command = "cmake -B build";
            } else {
                // Ignore the others for now
            }
        }

        parse_configuration(instructions, command, configurations, toolchain, context);
    }

    // Reset user config context
    context.user_config_context.reset(user_config_context);

    // Stages
    DEBUG("- Stages for package: " + package_name);
    std::string threads = global_config::get_num_proc();

    if (pkg_config["cheese"]["build"]["stages"]) {
        YAML::Node stages = pkg_config["cheese"]["build"]["stages"];

        for (const auto& stage : stages) {
            std::string stage_target;
            if (stage["target"].IsScalar()) {
                stage_target = parse_scalar(stage["target"].as<std::string>(), context);
            } else if (stage["target"].IsNull()) {
                stage_target = "";  // Default to empty string if not specified
            } else {
                ERROR("Invalid target type in stage: " + stage["target"].Type());
                exit(EXIT_FAILURE);
            }

            DEBUG("Stage target: '" + stage_target + "'");

            bool multithreaded;
            if (stage["multithreaded"] && stage["multithreaded"].IsScalar()) {
                multithreaded = stage["multithreaded"].as<bool>();
            } else {
                multithreaded = true;  // Default to true if not specified
            }

            std::string stage_cmd;

            if (stage["configurations"] && stage["configurations"]["command"]) {
                stage_cmd = stage["configurations"]["command"].as<std::string>();
            } else {
                if (toolchain == "autotools" || toolchain == "makefile") {
                    stage_cmd = "make";

                    if (multithreaded && !threads.empty()) {
                        stage_cmd += " -j" + threads;
                    }

                    if (!stage_target.empty()) {
                        stage_cmd += " " + stage_target;
                    }
                } else if (toolchain == "cmake") {
                    stage_cmd = "cmake --build build";

                    if (multithreaded && !threads.empty()) {
                        stage_cmd += " --parallel " + threads;
                    }

                    if (!stage_target.empty()) {
                        stage_cmd += " --target " + stage_target;
                    }
                } else {
                    // Ignore the others for now
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
                context.user_config_context.reset(user_config_context_config);

                parse_configuration(instructions, stage_cmd, stage_configurations, "", context);
            } else {
                instructions.push_back(stage_cmd);
            }
        }
    }

    // Reset user config context
    context.user_config_context.reset(user_config_context);

    // Postprocessing
    DEBUG("- Postprocessing for package: " + package_name);
    if (pkg_config["cheese"]["build"]["postprocessing"]) {
        std::string postprocessing = parse_scalar(
            pkg_config["cheese"]["build"]["postprocessing"].as<std::string>(), context);
        instructions.push_back(postprocessing);
    }

    return instructions;
}
