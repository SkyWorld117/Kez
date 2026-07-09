#pragma once

#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <string>

/**
 * @brief Check whether a YAML node contains a given top-level key.
 *
 * Convenience wrapper around YAML::Node::operator[] that avoids the need to
 * compare the result to YAML::Node() by hand.  Returns false when @p key does
 * not exist in @p node; returns true when it does (even if the value is null
 * or empty).
 *
 * @param node  The YAML node (typically a map) to search.
 * @param key   The key to look up.
 * @return true if @p key is present in @p node; false otherwise.
 *
 * @see yaml_scalar
 * @see yaml_boolean
 */
bool yaml_has(const YAML::Node& node, const std::string& key);

/**
 * @brief Extract and return a scalar string from a YAML node.
 *
 * Asserts (via ERROR) that @p node is a scalar; terminates the program with a
 * fatal message if it is not.  The @p description parameter is used solely to
 * enrich the error message so the user can identify which value is malformed.
 *
 * @param node        The YAML node that is expected to hold a scalar value.
 * @param description A human-readable label for the scalar (e.g. the YAML key
 *                    name or the semantic role of the value) that will appear
 *                    in the error message on failure.
 * @return The scalar value as a std::string.
 *
 * @warning Terminates the process with exit(EXIT_FAILURE) if @p node is not
 *          a scalar.
 *
 * @see yaml_has
 * @see yaml_boolean
 */
std::string yaml_scalar(const YAML::Node& node, const std::string& description);

/**
 * @brief Extract and return a boolean from a YAML node.
 *
 * Asserts (via ERROR) that @p node is a boolean; terminates the program with a
 * fatal message if it is not.  The @p description parameter is used solely to
 * enrich the error message so the user can identify which value is malformed.
 *
 * @param node        The YAML node that is expected to hold a boolean value.
 * @param description A human-readable label for the boolean (e.g. the YAML key
 *                    name or the semantic role of the value) that will appear
 *                    in the error message on failure.
 * @return The boolean value.
 *
 * @warning Terminates the process with exit(EXIT_FAILURE) if @p node is not
 *          a boolean.
 *
 * @see yaml_has
 * @see yaml_scalar
 */
bool yaml_boolean(const YAML::Node& node, const std::string& description);

/**
 * @brief Load a YAML file with process-lifetime caching.
 *
 * Parses the file at @p path and returns the root YAML::Node.  The result is
 * cached by the real (canonical, absolute) path so that repeated calls for the
 * same file never re-parse it.  If the file cannot be read or parsed the
 * program terminates with a fatal error message.
 *
 * The cache lives for the entire process lifetime; there is no mechanism to
 * evict entries or force a re-read.
 *
 * @param path  Filesystem path to the YAML file.  Will be resolved to an
 *              absolute path internally.
 * @return The root YAML::Node of the parsed document.
 *
 * @warning Terminates the process with exit(EXIT_FAILURE) on I/O or parse
 *          errors.
 */
YAML::Node cached_yaml_load(const std::filesystem::path& path);
