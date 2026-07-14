#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <utils/colored_io.hpp>
#include <utils/file_utils.hpp>

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

void write_yaml(const YAML::Node& node, const std::string& path,
                const std::string& success_message) {
    write_yaml(node, path);
    SUCCESS(success_message);
}

void write_yaml_atomic(const YAML::Node& node, const std::string& path) {
    const std::filesystem::path target(path);
    const std::filesystem::path dir = target.parent_path();

    // Ensure the target directory exists.
    std::filesystem::create_directories(dir);

    // Write to a temporary file next to the target, then atomically rename.
    const std::filesystem::path tmp_path =
        dir / (std::string(".tmp_") + target.filename().string());

    YAML::Emitter out;
    out << node;

    {
        std::ofstream ofs(tmp_path);
        if (!ofs) {
            ERROR("Failed to create temporary file: " + tmp_path.string());
            exit(EXIT_FAILURE);
        }
        ofs << out.c_str();
        ofs << std::endl;
    }

    std::error_code ec;
    std::filesystem::rename(tmp_path, target, ec);
    if (ec) {
        ERROR("Failed to rename temporary file to '" + path + "': " + ec.message());
        // Try to clean up the temp file.
        std::filesystem::remove(tmp_path, ec);
        exit(EXIT_FAILURE);
    }
}

void fs_create_dirs(const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::create_directories(path, ec) && ec) {
        ERROR("Failed to create directory '" + path.string() + "': " + ec.message());
        exit(EXIT_FAILURE);
    }
}

void fs_remove_all(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
    if (ec) {
        ERROR("Failed to remove '" + path.string() + "': " + ec.message());
        exit(EXIT_FAILURE);
    }
}
