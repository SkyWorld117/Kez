#include <colors/colored_io.hpp>
#include <filesystem>
#include <global_config.hpp>

std::filesystem::path prefix_path = std::filesystem::path(getenv("FROMAGER_WORKDIR"));
std::filesystem::path config_path = prefix_path / "config.yaml";
YAML::Node config                 = YAML::LoadFile(config_path.string())["fromager"];

std::string get_path(const std::string& target) {
    if (!config["paths"]) {
        ERROR("Illformed configuration: 'paths' section is missing");
        exit(EXIT_FAILURE);
    } else if (!config["paths"][target]) {
        ERROR("Illformed configuration: path for '" + target + "' is missing");
        exit(EXIT_FAILURE);
    } else {
        return (prefix_path / config["paths"][target].as<std::string>()).string();
    }
}

std::string get_num_proc() {
    if (!config["n_proc_for_build"]) {
        ERROR("Illformed configuration: 'n_proc_for_build' is missing");
        exit(EXIT_FAILURE);
    } else {
        return config["n_proc_for_build"].as<std::string>();
    }
}

std::string get_default_compiler() {
    if (!config["default_compiler"]) {
        ERROR("Illformed configuration: 'default_compiler' is missing");
        exit(EXIT_FAILURE);
    } else {
        return config["default_compiler"].as<std::string>();
    }
}

std::string get_default_mpi() {
    if (!config["default_mpi"]) {
        ERROR("Illformed configuration: 'default_mpi' is missing");
        exit(EXIT_FAILURE);
    } else {
        return config["default_mpi"].as<std::string>();
    }
}