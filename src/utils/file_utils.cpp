#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <utils/colored_io.hpp>
#include <utils/file_utils.hpp>

/**
 * @brief Read an entire file into a string.
 *
 * Opens the file at @p path and reads its full contents into a std::string
 * via the underlying streambuf.  Unlike the other utilities in this file
 * this function is silent on failure: if the file cannot be opened (e.g.
 * it does not exist or the process lacks read permissions) an empty string
 * is returned and no error message is printed.
 *
 * @param path  Filesystem path to the file to read.
 * @return The full content of the file as a std::string, or an empty string
 *         if the file could not be opened.
 *
 * @note This function does NOT call exit() or ERROR() on failure; callers
 *       must check the return value to distinguish "file is empty" from
 *       "file could not be read".
 */
std::string read_file(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs) {
        return "";
    }
    std::ostringstream buffer;
    buffer << ifs.rdbuf();
    return buffer.str();
}

/**
 * @brief Serialise a YAML node to a file, creating parent directories as
 *        needed.
 *
 * Emits the YAML::Node to the given path using a YAML::Emitter.  If @p path
 * is absolute, intervening directories are created automatically; if it is
 * relative, directories are created relative to the current working directory.
 *
 * If the output file cannot be created or opened for writing, the function
 * prints an error via ERROR() and terminates the process with a non-zero
 * exit code.  No success message is printed on the normal path.
 *
 * @param node  The YAML::Node to serialise.
 * @param path  Filesystem path where the YAML output should be written.
 *
 * @warning The function calls exit(EXIT_FAILURE) if the output file cannot
 *          be created or opened for writing.  It will never return normally
 *          in that case.
 *
 * @see write_yaml(const YAML::Node&, const std::string&, const std::string&)
 *      Overload that additionally prints a success message via SUCCESS().
 */
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

/**
 * @brief Serialise a YAML node to a file, create parent directories, and
 *        print a success message.
 *
 * Behaves identically to the two-argument overload of write_yaml() and then
 * prints @p success_message via the SUCCESS() macro (typically a green
 * "[S]: ..." line on stdout).
 *
 * @param node             The YAML::Node to serialise.
 * @param path             Filesystem path where the YAML output should be
 *                         written.
 * @param success_message  Human-readable message printed on success (e.g.
 *                         "Configuration written to /path/to/config.yaml").
 *
 * @warning The function calls exit(EXIT_FAILURE) if the output file cannot
 *          be created or opened for writing (see the two-argument overload).
 *          It will never return normally in that case.
 */
void write_yaml(const YAML::Node& node, const std::string& path,
                const std::string& success_message) {
    write_yaml(node, path);
    SUCCESS(success_message);
}
