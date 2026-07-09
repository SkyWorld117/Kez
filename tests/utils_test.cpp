#include <gtest/gtest.h>
#include <yaml-cpp/yaml.h>

#include <utils/bash_utils.hpp>
#include <utils/yaml_utils.hpp>

/**
 * @brief Verifies that shell quoting functions handle embedded quotes and
 *        special characters correctly.
 *
 * Tests shell_single_quote() with a value containing a single quote, ensuring
 * the canonical Bourne-shell escape sequence ('\\'') is emitted correctly.
 * Tests shell_double_quote() with a value containing a backslash, a literal
 * double quote, and a ${...} expression, confirming that the backslash and
 * double quote are escaped while the dollar-brace pattern is preserved so
 * that the shell expands the variable at runtime.
 *
 * Edge cases covered:
 *   - Single-quoted string with an embedded single quote (requires break and
 *     re-entering the quoted region).
 *   - Double-quoted string with embedded backslash, double quote, and a
 *     ${VARIABLE} form that must not be escaped.
 */
TEST(BashUtils, QuotesShellValuesWithoutSuppressingDoubleQuotedExpansion) {
    EXPECT_EQ(shell_single_quote("path with ' quote"), "'path with '\\'' quote'");
    EXPECT_EQ(shell_double_quote("value \\\" ${PATH}"), "\"value \\\\\\\" ${PATH}\"");
}

/**
 * @brief Verifies that YAML utility helpers correctly read scalar strings and
 *        boolean values from a YAML map, and properly report missing keys.
 *
 * Loads a minimal YAML document with a string value and a boolean key, then
 * checks:
 *   - yaml_has() returns true for an existing key and false for a missing one.
 *   - yaml_scalar() extracts the correct string for an existing key.
 *   - yaml_boolean() parses a YAML boolean ("true") into the expected value.
 *
 * Edge cases covered:
 *   - Map entry with a non-existent key (verified via yaml_has).
 *   - Boolean value parsed from the YAML string representation "true".
 */
TEST(YamlUtils, ReadsMapScalarsAndBooleans) {
    const YAML::Node document = YAML::Load("value: text\nenabled: true\n");

    EXPECT_TRUE(yaml_has(document, "value"));
    EXPECT_FALSE(yaml_has(document, "missing"));
    EXPECT_EQ(yaml_scalar(document["value"], "value"), "text");
    EXPECT_TRUE(yaml_boolean(document["enabled"], "enabled"));
}
