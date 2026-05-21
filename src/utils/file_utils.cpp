#include <fstream>
#include <sstream>
#include <utils/colored_io.hpp>
#include <utils/file_utils.hpp>
#include <yaml-cpp/yaml.h>
#include <filesystem>

std::string read_file(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs) {
        return "";
    }
    std::ostringstream buffer;
    buffer << ifs.rdbuf();
    return buffer.str();
}

void write_yaml(const YAML::Node& node, const std::string& path) {
    std::filesystem::path fs_path(path);
    if (fs_path.is_absolute()) {
        std::filesystem::create_directories(fs_path.parent_path());
    } else {
        std::filesystem::path abs_path = std::filesystem::current_path() / fs_path;
        std::filesystem::create_directories(abs_path.parent_path());
    }

    YAML::Emitter out;
    out << node;

    std::ofstream ofs(path);
    if (!ofs) {
        ERROR("Failed to create file: " + path);
        exit(EXIT_FAILURE);
    }

    ofs << out.c_str();
    ofs << std::endl;
    ofs.close();
}

void write_yaml(const YAML::Node& node, const std::string& path, const std::string& success_message) {
    write_yaml(node, path);
    SUCCESS(success_message);
}