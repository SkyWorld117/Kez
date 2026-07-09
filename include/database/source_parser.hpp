#pragma once

#include <yaml-cpp/yaml.h>

#include <database/config.hpp>
#include <database/parser_context.hpp>
#include <string>
#include <vector>

/**
 * @brief Parse a ``source`` block from a package recipe YAML node into a
 *        @ref Source struct.
 *
 * This function validates and extracts the ``source`` metadata of a package
 * recipe. The input node must be a mapping with the following keys:
 *
 * Key       | Required? | Description
 * --------- | --------- | -----------
 * ``type``  | Yes       | One of ``"git"``, ``"tarball"``, ``"zip"``, or ``"script"``.
 * ``url``   | Conditional | Required when ``type`` is ``"git"``; otherwise optional.
 * ``releases`` | Yes    | A non-empty sequence of release entries.
 *
 * Each element of ``releases`` is itself a mapping supporting the keys:
 *
 * Key         | Required? | Description
 * ----------- | --------- | -----------
 * ``version`` | Yes       | The release version string. Must be non-empty and unique within the source.
 * ``url``     | Conditional | Required when ``type`` is ``"tarball"`` or ``"zip"``; forbidden (ignored) for ``"script"``.
 * ``tag``     | Conditional | Required when ``type`` is ``"git"``; ignored otherwise.
 *
 * Validation performed (all failures are fatal via @ref fail_config):
 * - The ``source`` node is a map.
 * - No unexpected keys are present (only ``type``, ``url``, ``releases``).
 * - ``type`` matches one of the four known values.
 * - ``url`` is present for git sources.
 * - ``releases`` is a non-empty sequence.
 * - Every release has a non-empty, unique ``version``.
 * - For git sources, every release has a ``tag``.
 * - For tarball/zip sources, every release has a ``url``.
 *
 * @param node    The YAML node representing the ``source`` block of a package
 *                recipe (e.g. ``database/<pkg>/latest.yaml``'s ``source``
 *                key).
 * @param path    A human-readable YAML path string used in error messages to
 *                identify the location of a problem (e.g. ``source``,
 *                ``source.releases[0].version``).
 * @param context The parser context carrying the recipe tree root path.
 *
 * @return A fully populated @ref Source struct. The function either returns
 *         a valid result or terminates the program via @ref fail_config on
 *         any validation error.
 *
 * @see Source
 * @see SourceType
 * @see Release
 * @see parse_scalar_sequence
 */
Source parse_source(const YAML::Node& node, const std::string& path,
                    const DatabaseParserContext& context);

/**
 * @brief Parse a YAML sequence node into a vector of strings.
 *
 * Each element of the sequence is expected to be a YAML scalar value; the
 * function iterates over all entries, converts each to a @c std::string,
 * and returns them in order.
 *
 * @param node    The YAML node that must be a sequence (e.g. a list of
 *                dependency names or configuration keys).
 * @param path    A human-readable YAML path string used in error messages
 *                (e.g. ``dependencies``, ``properties[2]``).
 * @param context The parser context carrying the recipe tree root path.
 *
 * @return A @c std::vector<std::string> containing the parsed scalar values
 *         in sequence order.
 *
 * @note If the node is not a YAML sequence, the function calls
 *       @ref expect_sequence which terminates the program via
 *       @ref fail_config.
 *
 * @see expect_sequence
 * @see parse_source
 */
std::vector<std::string> parse_scalar_sequence(const YAML::Node& node, const std::string& path,
                                               const DatabaseParserContext& context);
