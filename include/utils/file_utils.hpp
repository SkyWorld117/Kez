#pragma once

#include <yaml-cpp/yaml.h>

#include <string>

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
