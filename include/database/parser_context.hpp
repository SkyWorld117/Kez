#pragma once

#include <filesystem>

/**
 * @brief Carries contextual information needed throughout the database
 *        parsing process.
 *
 * DatabaseParserContext is a plain data holder passed to all sub-parsers
 * (source parser, build parser, config parser, condition parser, etc.) so
 * they can resolve relative paths and access the root of the package-
 * recipe tree without relying on global state.
 *
 * Users of this struct create an instance once per parse run (typically in
 * the top-level database parser), populate the @ref source_path member,
 * and forward it by const-reference to every sub-parser invocation.
 */
struct DatabaseParserContext {
    /**
     * @brief Absolute or relative path to the root directory containing the
     *        package recipe YAML files (e.g. the project's ``database/``
     *        folder).
     *
     * Sub-parsers use this path as a base when they need to open additional
     * files referenced from a recipe — for example, patch files listed in
     * a package's metadata or auxiliary configuration YAML that lives
     * alongside the recipe.
     *
     * The path is expected to point to an existing directory; parsers may
     * call @c std::filesystem::exists on derived paths and fail with an
     * error if the expected file is not found.
     */
    std::filesystem::path source_path;
};
