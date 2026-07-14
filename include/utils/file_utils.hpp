#pragma once

#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <string>
#include <system_error>

/**
 * @brief Read an entire file into a string.
 *
 * Opens the file at the given path and reads its full contents into a
 * std::string via the underlying streambuf.  If the file cannot be opened
 * (e.g. it does not exist or the process lacks read permissions) an empty
 * string is returned silently -- no error is printed and no message is
 * emitted to stderr.
 *
 * @param path  Filesystem path to the file to read.
 * @return The full content of the file as a std::string, or an empty string
 *         if the file could not be opened.
 *
 * @note This function does NOT call exit() or ERROR() on failure; callers
 *       must check the return value to distinguish "file is empty" from
 *       "file could not be read".
 */
std::string read_file(const std::string& path);

/**
 * @brief Serialise a YAML node to a file, creating parent directories as
 *        needed.
 *
 * Emits the YAML::Node to the given path using a YAML::Emitter.  If the path
 * is absolute, intervening directories are created automatically; if it is
 * relative, directories are created relative to the current working directory.
 *
 * On failure to open the file for writing the program terminates immediately
 * with a non-zero exit code after printing an error message via ERROR().
 * No success message is printed on the normal path.
 *
 * @param node  The YAML::Node to serialise.
 * @param path  Filesystem path where the YAML output should be written.
 *
 * @warning The function calls exit(EXIT_FAILURE) if the output file cannot
 *          be created or opened for writing.  It will never return normally
 *          in that case.
 *
 * @see write_yaml(const YAML::Node&, const std::string&, const std::string&)
 *      Overload that additionally prints a success message.
 */
void write_yaml(const YAML::Node& node, const std::string& path);

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
 *          be created or opened for writing.  It will never return normally
 *          in that case.
 *
 * @see write_yaml(const YAML::Node&, const std::string&)
 *      Core serialisation overload without the success notification.
 */
void write_yaml(const YAML::Node& node, const std::string& path,
                const std::string& success_message);

/**
 * @brief Serialise a YAML node to a file using a temporary file and atomic
 *        rename.
 *
 * Writes content to a temporary file in the same directory as @p path, then
 * atomically renames it over @p path via @c std::filesystem::rename.  This
 * prevents partial/corrupt output if the process is interrupted mid-write.
 *
 * On failure to create the temporary file, write to it, or rename it, the
 * program terminates with a non-zero exit code after printing an error
 * message via ERROR().
 *
 * @param node  The YAML::Node to serialise.
 * @param path  Target filesystem path for the YAML output.
 */
void write_yaml_atomic(const YAML::Node& node, const std::string& path);

// -----------------------------------------------------------------------
// Non-throwing filesystem helpers
//
// These wrappers accept a std::error_code parameter so that callers never
// hit a std::filesystem exception.  Mutation helpers that must succeed on
// normal paths print a descriptive error and terminate via ERROR().
// -----------------------------------------------------------------------

/**
 * @brief Wrapper around std::filesystem::is_regular_file that uses
 *        std::error_code and never throws.
 */
inline bool fs_regular_file(const std::filesystem::path& path) noexcept {
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec);
}

/**
 * @brief Wrapper around std::filesystem::is_directory that uses
 *        std::error_code and never throws.
 */
inline bool fs_directory(const std::filesystem::path& path) noexcept {
    std::error_code ec;
    return std::filesystem::is_directory(path, ec);
}

/**
 * @brief Wrapper around std::filesystem::exists that uses
 *        std::error_code and never throws.
 */
inline bool fs_exists(const std::filesystem::path& path) noexcept {
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

/**
 * @brief Create a directory and all its parents, printing an error and
 *        terminating on failure.
 *
 * Never throws; uses std::error_code internally.
 */
void fs_create_dirs(const std::filesystem::path& path);

/**
 * @brief Remove a file or directory and all its contents, printing an
 *        error and terminating on failure.
 *
 * Never throws; uses std::error_code internally.
 */
void fs_remove_all(const std::filesystem::path& path);
