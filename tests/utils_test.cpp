#include <gtest/gtest.h>
#include <yaml-cpp/yaml.h>

#include <utils/bash_utils.hpp>
#include <utils/yaml_utils.hpp>

TEST(BashUtils, QuotesShellValuesWithoutSuppressingDoubleQuotedExpansion) {
    EXPECT_EQ(shell_single_quote("path with ' quote"), "'path with '\\'' quote'");
    EXPECT_EQ(shell_double_quote("value \\\" ${PATH}"), "\"value \\\\\\\" ${PATH}\"");
}

TEST(YamlUtils, ReadsMapScalarsAndBooleans) {
    const YAML::Node document = YAML::Load("value: text\nenabled: true\n");

    EXPECT_TRUE(yaml_has(document, "value"));
    EXPECT_FALSE(yaml_has(document, "missing"));
    EXPECT_EQ(yaml_scalar(document["value"], "value"), "text");
    EXPECT_TRUE(yaml_boolean(document["enabled"], "enabled"));
}
