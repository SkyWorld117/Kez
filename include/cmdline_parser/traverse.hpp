#pragma once

#include <yaml-cpp/yaml.h>

#include <string>

/**
 * @brief Assign a scalar value to a node located by a dot-separated path.
 *
 * Walks the YAML tree rooted at @p node following the segments of @p path.
 * If a path segment addresses a **sequence** node, the segment is interpreted
 * as one of:
 *   - A numeric index (e.g. ``0``) — selects that sequence element.
 *   - A string value (e.g. ``libtirpc``) — walks the sequence and selects
 *     the first map whose ``name`` or ``target`` field matches the given
 *     value.
 *
 * The final segment is assigned @p value as a scalar.  If any intermediate
 * path segment does not exist, the function terminates the program with an
 * error message.
 *
 * @param path  Dot-separated path describing the location in the YAML tree
 *              (e.g. ``"application.version"``).  Sequence elements are
 *              addressed by their numeric index (e.g.
 *              ``"build.stages.0.configurations"``) or by the value of their
 *              ``name`` / ``target`` field.
 * @param value The scalar string to assign at the resolved location.
 * @param node  The root YAML node to start the traversal from.
 *
 * @warning Terminates the program if any path segment cannot be resolved.
 *          No intermediate nodes are created; every segment except the final
 *          one must already exist.
 */
void traverse(const std::string& path, const std::string& value, YAML::Node node);
