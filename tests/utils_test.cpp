/**
 * @file utils_test.cpp
 * @brief Unit tests for shared string, shell, file, and YAML helpers.
 */

#include <gtest/gtest.h>
#include <unistd.h>
#include <yaml-cpp/yaml.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <utils/bash_utils.hpp>
#include <utils/file_utils.hpp>
#include <utils/string_utils.hpp>
#include <utils/yaml_utils.hpp>
#include <vector>

namespace {

    class TemporaryUtilsFiles : public ::testing::Test {
       protected:
        void SetUp() override {
            const ::testing::TestInfo* info =
                ::testing::UnitTest::GetInstance()->current_test_info();
            path_ = std::filesystem::temp_directory_path() /
                    ("kez-utils-test-" + std::to_string(getpid()) + "-" + info->name());
            std::filesystem::remove_all(path_);
            std::filesystem::create_directories(path_);
        }

        void TearDown() override { std::filesystem::remove_all(path_); }

        void write(const std::filesystem::path& path, const std::string& contents) const {
            std::ofstream output(path);
            ASSERT_TRUE(output.good());
            output << contents;
        }

        std::filesystem::path path_;
    };

    TEST(StringUtils, SplitsOnSingleAndMultipleDelimiters) {
        EXPECT_EQ(split("alpha:beta:gamma", ':'),
                  (std::vector<std::string> {"alpha", "beta", "gamma"}));
        EXPECT_EQ(split(",alpha,,beta", ','), (std::vector<std::string> {"alpha", "beta"}));
        EXPECT_EQ(split("alpha,beta;gamma", std::vector<char> {',', ';'}),
                  (std::vector<std::string> {"alpha", "beta", "gamma"}));
        EXPECT_TRUE(split("", ':').empty());
    }

    TEST(StringUtils, KeepsDelimiterTokens) {
        EXPECT_EQ(split_keep_delimiters("a<=b", std::vector<char> {'<', '='}),
                  (std::vector<std::string> {"a", "<", "=", "b"}));
        EXPECT_EQ(split_keep_delimiters("plain", std::vector<char> {'.'}),
                  (std::vector<std::string> {"plain"}));
        EXPECT_TRUE(split_keep_delimiters("", std::vector<char> {'.'}).empty());
    }

    TEST(StringUtils, ClassifiesAlphabeticAndNumericText) {
        EXPECT_TRUE(is_alphabetic("Kez"));
        EXPECT_TRUE(is_alphabetic('Z'));
        EXPECT_FALSE(is_alphabetic("Kez2"));
        EXPECT_FALSE(is_alphabetic('7'));

        EXPECT_TRUE(is_numeric("012345"));
        EXPECT_TRUE(is_numeric('7'));
        EXPECT_FALSE(is_numeric("12.5"));
        EXPECT_FALSE(is_numeric('x'));
    }

    TEST(StringUtils, ComparesNumericAndSuffixedVersions) {
        EXPECT_LT(compare_versions("1.9", "1.10"), 0);
        EXPECT_GT(compare_versions("2.0", "1.99"), 0);
        EXPECT_LT(compare_versions("1.0a", "1.0b"), 0);
        EXPECT_LT(compare_versions("1.0", "1.0.1"), 0);
        EXPECT_EQ(compare_versions("13.4.0", "13.4.0"), 0);
    }

    TEST(StringUtils, HandlesAnsiCodesAndWhitespace) {
        EXPECT_EQ(get_length_without_color("plain"), 5);
        EXPECT_EQ(get_length_without_color("\033[31mred\033[0m text"), 8);
        EXPECT_EQ(trim(" \t\n value \r"), "value");
        EXPECT_EQ(trim(" \n\t\r "), "");
        EXPECT_EQ(trim("already-trimmed"), "already-trimmed");
    }

    TEST(StringUtils, AppendsUniqueValuesAndJoinsThem) {
        std::vector<std::string> values {"first"};
        append_unique(values, "second");
        append_unique(values, "first");
        append_unique(values, "");

        EXPECT_EQ(values, (std::vector<std::string> {"first", "second"}));
        EXPECT_EQ(join(values), "first second");
        EXPECT_EQ(join(values, ":"), "first:second");
        EXPECT_EQ(join({}, ":"), "");
    }

    TEST(StringUtils, ValidatesShellAssignmentNames) {
        EXPECT_TRUE(is_shell_assignment("PATH"));
        EXPECT_TRUE(is_shell_assignment("_KEZ_2"));
        EXPECT_FALSE(is_shell_assignment(""));
        EXPECT_FALSE(is_shell_assignment("2FAST"));
        EXPECT_FALSE(is_shell_assignment("lowercase"));
        EXPECT_FALSE(is_shell_assignment("HAS-DASH"));
    }

    TEST(BashUtils, QuotesShellValuesWithoutSuppressingDoubleQuotedExpansion) {
        EXPECT_EQ(shell_single_quote("path with ' quote"), "'path with '\\'' quote'");
        EXPECT_EQ(shell_double_quote("value \\\" ${PATH}"), "\"value \\\\\\\" ${PATH}\"");
        EXPECT_EQ(shell_single_quote(""), "''");
        EXPECT_EQ(shell_double_quote(""), "\"\"");
    }

    TEST(BashUtils, ReadsEnvironmentValuesAndDefaults) {
        constexpr const char* name = "KEZ_UTILS_TEST_VALUE";
        setenv(name, "configured value", 1);
        EXPECT_EQ(get_env_var(name), "configured value");
        EXPECT_EQ(get_env_var_noerr(name, "fallback"), "configured value");

        unsetenv(name);
        EXPECT_EQ(get_env_var_noerr(name), "");
        EXPECT_EQ(get_env_var_noerr(name, "fallback"), "fallback");
        EXPECT_EXIT(static_cast<void>(get_env_var(name)), ::testing::ExitedWithCode(EXIT_FAILURE),
                    "is not set");
    }

    TEST(YamlUtils, ReadsMapScalarsAndBooleans) {
        const YAML::Node document =
            YAML::Load("value: text\nenabled: true\ndisabled: FALSE\nnull_value:\n");

        EXPECT_TRUE(yaml_has(document, "value"));
        EXPECT_TRUE(yaml_has(document, "null_value"));
        EXPECT_FALSE(yaml_has(document, "missing"));
        EXPECT_FALSE(yaml_has(document["value"], "nested"));
        EXPECT_EQ(yaml_scalar(document["value"], "value"), "text");
        EXPECT_TRUE(yaml_boolean(document["enabled"], "enabled"));
        EXPECT_FALSE(yaml_boolean(document["disabled"], "disabled"));
    }

    TEST(YamlUtils, RejectsInvalidScalarAndBooleanValues) {
        EXPECT_EXIT(static_cast<void>(yaml_scalar(YAML::Load("[one]"), "items")),
                    ::testing::ExitedWithCode(EXIT_FAILURE), "items must be a scalar");
        EXPECT_EXIT(static_cast<void>(yaml_boolean(YAML::Load("yes"), "enabled")),
                    ::testing::ExitedWithCode(EXIT_FAILURE), "enabled must be true or false");
    }

    TEST_F(TemporaryUtilsFiles, ReadsWritesAndCreatesNestedYamlFiles) {
        const std::filesystem::path plain_file = path_ / "plain.txt";
        write(plain_file, "first line\nsecond line\n");
        EXPECT_EQ(read_file(plain_file.string()), "first line\nsecond line\n");
        EXPECT_EQ(read_file((path_ / "missing.txt").string()), "");

        YAML::Node document;
        document["name"]                      = "kez";
        document["enabled"]                   = true;
        const std::filesystem::path yaml_file = path_ / "nested" / "config.yaml";
        write_yaml(document, yaml_file.string());

        ASSERT_TRUE(std::filesystem::is_regular_file(yaml_file));
        const YAML::Node loaded = YAML::LoadFile(yaml_file.string());
        EXPECT_EQ(loaded["name"].as<std::string>(), "kez");
        EXPECT_TRUE(loaded["enabled"].as<bool>());
    }

    TEST_F(TemporaryUtilsFiles, CachesYamlByNormalizedAbsolutePath) {
        const std::filesystem::path yaml_file = path_ / "cached.yaml";
        write(yaml_file, "value: first\n");
        const YAML::Node first = cached_yaml_load(path_ / "." / "cached.yaml");
        ASSERT_EQ(first["value"].as<std::string>(), "first");

        write(yaml_file, "value: second\n");
        const YAML::Node cached = cached_yaml_load(std::filesystem::absolute(yaml_file));
        EXPECT_EQ(cached["value"].as<std::string>(), "first");
    }

}  // namespace
