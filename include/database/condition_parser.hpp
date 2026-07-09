#pragma once

#include <yaml-cpp/yaml.h>

#include <database/parser_context.hpp>
#include <string>

/**
 * @brief Validates a condition expression found in a package recipe.
 *
 * Checks the syntactic and semantic validity of a condition string (e.g. an
 * ``if:`` or ``unless:`` clause in a recipe's build or configuration stanza).
 * The function evaluates whether the expression conforms to the supported
 * condition grammar (operators, identifiers, literals) and that any
 * referenced variables or features are known to the parser.
 *
 * @param expression The raw condition string to validate (e.g. ``"arch ==
 *                   x86_64"`` or ``"compiler.family == 'gcc'"``).
 * @param node       The YAML node that owns the condition. Used for
 *                   producing informative error messages that include the
 *                   surrounding context (line number, key path).
 * @param path       Dot-separated YAML key path pointing to the location
 *                   of the condition within the recipe (e.g.
 *                   ``"build.phases.0.if"``). Included in diagnostics so
 *                   the user can pinpoint the source of a validation error.
 * @param context    Database-parsing context carrying the recipe-tree root
 *                   path and other state shared across sub-parsers.
 *
 * @warning If the expression is malformed or references an unknown variable,
 *          the function prints an error message via @c ERROR (from
 *          ``colored_io.hpp``) and terminates the program with a non-zero
 *           exit code. It never returns normally on failure.
 *
 * @see validate_templates  Sister function that validates template
 *                          substitutions inside the same recipe node.
 * @see build_parser.hpp    Caller that invokes condition validation on
 *                          build-phase entries.
 * @see config_parser.hpp   Caller that invokes condition validation on
 *                          configuration options.
 */
void validate_condition(const std::string& expression, const YAML::Node& node,
                        const std::string& path, const DatabaseParserContext& context);

/**
 * @brief Validates all template expressions ( ``{{ ... }}`` ) within a YAML
 *        recipe node.
 *
 * Recursively walks the given YAML node and checks every string value for
 * template substitution syntax. Each template expression is verified for
 * well-formedness — balanced delimiters, valid variable names, supported
 * filters, etc. — and that the referenced variables appear in the expected
 * context (e.g. the package's own metadata or the global configuration).
 *
 * @param node    The root YAML node (typically a complete recipe or a large
 *                substanza such as ``build`` or ``config``) whose string
 *                leaves are scanned for template markers.
 * @param path    Dot-separated YAML key path leading to @p node, used as
 *                a prefix when reporting the location of any malformed
 *                template (e.g. ``"source.urls.0"``).
 * @param context Database-parsing context carrying the recipe-tree root
 *                path and other state shared across sub-parsers.
 *
 * @warning If any template expression fails validation, the function prints
 *          an error message via @c ERROR (from ``colored_io.hpp``) and
 *          terminates the program with a non-zero exit code. It never
 *          returns normally on failure.
 *
 * @note  This function does **not** perform template expansion; it only
 *        validates that all templates present in the node are syntactically
 *        correct and reference known variables. Expansion happens later in
 *        the pipeline (@see user_config_parser.hpp or the runtime
 *        template resolver).
 *
 * @see validate_condition  Sister function that validates condition
 *                          expressions ( ``if:`` / ``unless:`` ) inside
 *                          the same recipe.
 * @see source_parser.hpp   Caller that invokes template validation on
 *                          source-definition nodes.
 * @see build_parser.hpp    Caller that invokes template validation on
 *                          build-definition nodes.
 */
void validate_templates(const YAML::Node& node, const std::string& path,
                        const DatabaseParserContext& context);
