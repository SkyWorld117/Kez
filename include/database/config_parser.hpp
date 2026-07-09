#pragma once

#include <yaml-cpp/yaml.h>

#include <database/database.hpp>
#include <database/parser_context.hpp>

/**
 * @brief Parse the top-level YAML document of a package recipe into a
 *        fully-populated PackageConfig object.
 *
 * This is the main entry point for the config-parsing sub-component of the
 * database layer. It expects @p document to be a YAML map whose single
 * top-level key is @c "recipe". Inside the recipe it recognizes the
 * following optional and required keys:
 *
 *   Required   | Description
 *   :--------- | :----------
 *   @c name    | Package name (non-empty string).
 *   @c type    | One of `package`, `system`, `compiler`, `mpi`, `vendor`,
 *               | `abstract`, or `external`.
 *
 *   Optional   | Delegated to
 *   :--------- | :----------
 *   @c source  | @c parse_source()        (source_parser.hpp)
 *   @c build   | @c parse_build()         (build_parser.hpp)
 *   @c dependencies       | parse_scalar_sequence()  (parser_utils.hpp)
 *   @c overrides         | parse_overrides() (internal helper)
 *   @c properties        | parse_properties()(internal helper)
 *   @c implementations   | parse_scalar_sequence()  (parser_utils.hpp)
 *   @c description       | parsed as optional string
 *   @c author            | parsed as optional string
 *   @c toolchain         | Selects the concrete PackageConfig subclass:
 *                         | `autotools` / `cmake` / `make` / absent -> Generic
 *
 * The function also validates every template expression present in the
 * document (see @c validate_templates()) and enforces that packages of type
 * @c PackageType::Abstract declare at least one implementation entry.
 *
 * @param document  The root YAML node of the recipe file. Must be a map
 *                  containing a @c "recipe" key.
 * @param context   Parsing context carrying the base path of the recipe
 *                  database. Used by sub-parsers when they need to resolve
 *                  relative file references, and by error-reporting helpers
 *                  to produce actionable diagnostic messages.
 *
 * @return A shared pointer to a const @c PackageConfig instance. The
 *         concrete dynamic type (GenericPackageConfig, AutotoolsPackageConfig,
 *         CMakePackageConfig, or MakePackageConfig) is determined by the
 *         @c toolchain field in the recipe.
 *
 * @warning The function terminates the program with @c exit(EXIT_FAILURE)
 *          via the @c ERROR() / @c fail_config() helpers on any malformed
 *          or semantically invalid input (missing required keys, unknown
 *          package type, empty name, duplicate properties, etc.).
 *          Exceptions are never thrown.
 *
 * @see parse_build()         (build_parser.hpp)  — parses the @c build key.
 * @see parse_source()        (source_parser.hpp) — parses the @c source key.
 * @see parse_condition()     (condition_parser.hpp) — validates condition
 *                              strings found in overrides and configurable
 *                              values.
 * @see parse_db_config()     (database.hpp)      — loads a YAML file from
 *                              disk and calls this function.
 */
PackageConfigPtr parse_config_document(const YAML::Node& document,
                                       const DatabaseParserContext& context);
