#include <gtest/gtest.h>
#include <unistd.h>

#include <cstdlib>
#include <database/database.hpp>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

    /**
     * @brief Test fixture that sets up a temporary, isolated database directory.
     *
     * Each test creates a fresh directory under the system's temporary path and
     * points KEZ_DB at it, so package recipes written during the test are the
     * only ones visible to the database layer. After the test completes, the
     * temporary directory is removed and any previously-set KEZ_DB is restored.
     *
     * The helper method write() lets tests populate one-shot recipe YAML files
     * with minimal boilerplate.
     */
    class TemporaryDatabase : public ::testing::Test {
       protected:
        /**
         * @brief Creates a temporary directory and sets KEZ_DB to point at it.
         *
         * Saves any existing KEZ_DB value so TearDown() can restore it.
         * Clears the database cache before each test so stale entries from
         * earlier tests do not interfere.
         */
        void SetUp() override {
            const char* current = std::getenv("KEZ_DB");
            if (current != nullptr) {
                previous_database_ = current;
            }
            path_ = std::filesystem::temp_directory_path() /
                    ("kez-database-test-" + std::to_string(getpid()));
            std::filesystem::remove_all(path_);
            std::filesystem::create_directories(path_);
            setenv("KEZ_DB", path_.c_str(), 1);
            clear_db_cache();
        }

        /**
         * @brief Removes the temporary directory and restores the original KEZ_DB.
         *
         * Clears the database cache to release any resources that may reference
         * the (now removed) temporary directory. If no KEZ_DB was set before the
         * test, the environment variable is unset.
         */
        void TearDown() override {
            clear_db_cache();
            std::filesystem::remove_all(path_);
            if (previous_database_.empty()) {
                unsetenv("KEZ_DB");
            } else {
                setenv("KEZ_DB", previous_database_.c_str(), 1);
            }
        }

        /**
         * @brief Writes a YAML recipe file into the temporary database directory.
         *
         * @param package  The package name (used as the subdirectory name under
         *                 the temporary KEZ_DB root).
         * @param filename The recipe file name, e.g. "latest.yaml" or a version
         *                 range file such as "1.0.0-1.9.9.yaml".
         * @param contents The YAML content to write. The test asserts that the
         *                 file was opened successfully; failures trigger a fatal
         *                 test error via ASSERT_TRUE.
         */
        void write(const std::string& package, const std::string& filename,
                   const std::string& contents) const {
            const std::filesystem::path directory = path_ / package;
            std::filesystem::create_directories(directory);
            std::ofstream output(directory / filename);
            ASSERT_TRUE(output.good());
            output << contents;
        }

        std::filesystem::path path_;     ///< Path to the temporary database root.
        std::string previous_database_;  ///< Saved KEZ_DB value, empty if none was set.
    };

    /**
     * @brief Verifies that version-range config files are selected correctly and
     *        that out-of-range versions fall back to latest.yaml.
     *
     * Creates two recipe files for "demo":
     *   - "latest.yaml" declares a cmake-based package named "demo".
     *   - "1.0.0-1.9.9.yaml" declares an autotools-based package named "demo-v1"
     *     covering versions 1.0.0 through 1.9.9.
     *
     * Tests:
     *   - A request for version 1.0.0 returns the version-range config ("demo-v1").
     *   - A request for version 1.9.9 (the end of the range) returns the same
     *     pointer (range inclusion is verified).
     *   - The returned config is an AutotoolsPackageConfig with the expected
     *     default configure command.
     *   - A request for version 2.0.0 (outside the defined range) falls back to
     *     latest.yaml, returning a CMakePackageConfig with the expected default
     *     commands.
     *   - Calling get_db_config("demo") with no version also falls back to the
     *     latest default.
     */
    TEST_F(TemporaryDatabase, SelectsInclusiveVersionRangesAndFallsBackToLatest) {
        write("demo", "latest.yaml", R"(
recipe:
  name: demo
  type: package
  toolchain: cmake
)");
        write("demo", "1.0.0-1.9.9.yaml", R"(
recipe:
  name: demo-v1
  type: package
  toolchain: autotools
)");

        PackageConfigPtr range_start = get_db_config("demo", "1.0.0");
        PackageConfigPtr range_end   = get_db_config("demo", "1.9.9");
        PackageConfigPtr latest      = get_db_config("demo", "2.0.0");

        EXPECT_EQ(range_start->name, "demo-v1");
        EXPECT_EQ(range_end, range_start);
        EXPECT_NE(dynamic_cast<const AutotoolsPackageConfig*>(range_start.get()), nullptr);
        EXPECT_EQ(range_start->default_configuration_command(), "./configure");
        EXPECT_EQ(latest->name, "demo");
        EXPECT_NE(dynamic_cast<const CMakePackageConfig*>(latest.get()), nullptr);
        EXPECT_EQ(latest->default_configuration_command(), "cmake -B build");
        EXPECT_EQ(latest->default_stage_command(BuildStage {"install", true, std::nullopt}, 4),
                  "cmake --install build");
        EXPECT_EQ(get_db_config("demo"), latest);
    }

    /**
     * @brief Verifies that every documented YAML section in a recipe is parsed
     *        into the correct typed C++ data structure.
     *
     * Writes a comprehensive "typed" recipe exercising source (Git), dependency
     * declarations, build configurations (options with conditions, environment
     * variables, stages), property conditions, and overrides. Then asserts that
     * each parsed field has the expected type, value, or size.
     *
     * Edge cases covered:
     *   - Source block with typed releases (version + tag).
     *   - Conditionally-enabled build option whose default flips based on a
     *     version condition.
     *   - Property with a conditional value action (append).
     *   - Override with a target expression and action.
     *   - Build stage with multithreaded disabled.
     */
    TEST_F(TemporaryDatabase, ConvertsEveryDocumentedSectionToTypedData) {
        write("typed", "latest.yaml", R"(
recipe:
  name: typed
  description: Typed package
  author: test
  type: package
  toolchain: make
  source:

    type: git
    url: https://example.invalid/repository.git
    releases:
      - version: 1.0.0
        tag: v1.0.0
  dependencies: [compiler]
  overrides:
    - target: ${compiler.cflags}
      action: append
      value: -O3
  build:
    preprocessing: prepare
    configurations:
      environment:
        - name: CC
          default: ${compiler.c}
      options:
        - name: FEATURE
          user_configurable: true
          enabled:
            default: false
            conditions:
              - condition: version ${typed.version}>=1.0.0
                value: true
          enabled_value:
            default: yes
    stages:
      - target: all
        multithreaded: false
  properties:
    prefix: ${typed.prefix}
    libs:
      default: -ltyped
      conditions:
        - condition: ${typed.config.FEATURE} true
          action: append
          value: -lfeature
)");

        PackageConfigPtr config = get_db_config("typed", "1.0.0");
        ASSERT_NE(dynamic_cast<const MakePackageConfig*>(config.get()), nullptr);
        ASSERT_TRUE(config->source.has_value());
        EXPECT_EQ(config->source->type, SourceType::Git);
        ASSERT_EQ(config->source->releases.size(), 1U);
        EXPECT_EQ(config->source->releases[0].tag, "v1.0.0");
        ASSERT_TRUE(config->build.has_value());
        ASSERT_TRUE(config->build->configurations.has_value());
        ASSERT_EQ(config->build->configurations->options.size(), 1U);
        const BuildOption& option = config->build->configurations->options.front();
        ASSERT_TRUE(option.enabled.has_value());
        EXPECT_EQ(option.enabled->default_value, false);
        ASSERT_EQ(option.enabled->conditions.size(), 1U);
        EXPECT_TRUE(option.enabled->conditions.front().value);
        ASSERT_EQ(config->build->stages.size(), 1U);
        EXPECT_EQ(config->default_stage_command(config->build->stages.front(), 4), "make all");
        ASSERT_EQ(config->properties.size(), 2U);
        EXPECT_TRUE(
            std::holds_alternative<ConfigurableValue<std::string>>(config->properties[1].data));
        ASSERT_EQ(config->overrides.size(), 1U);
        EXPECT_EQ(config->overrides.front().action, ValueAction::Append);
    }

    /**
     * @brief Verifies that unrecognized YAML keys at any nesting level are
     *        silently ignored (the recipe still parses) and a warning is emitted.
     *
     * Writes a recipe containing an "unexpected" key inside a build option. The
     * test asserts that:
     *   - The option is still parsed (name is present).
     *   - The captured stdout contains the string "ignoring unexpected key".
     *   - The warning includes the full JSON pointer path to the unknown key
     *     ("recipe.build.configurations.options[0].unexpected") and the prefix
     *     used by the project's warning infrastructure ("[W]:").
     */
    TEST_F(TemporaryDatabase, IgnoresUnexpectedKeysAtAnySchemaLevelAndEmitsWarning) {
        write("invalid", "latest.yaml", R"(
recipe:
  name: invalid
  type: package
  build:
    configurations:
      options:
        - name: feature
          unexpected: true
)");

        testing::internal::CaptureStdout();
        PackageConfigPtr config   = get_db_config("invalid", "latest");
        const std::string warning = testing::internal::GetCapturedStdout();

        ASSERT_TRUE(config->build.has_value());
        ASSERT_TRUE(config->build->configurations.has_value());
        ASSERT_EQ(config->build->configurations->options.size(), 1U);
        EXPECT_EQ(config->build->configurations->options.front().name, "feature");
        EXPECT_NE(warning.find("ignoring unexpected key"), std::string::npos);
        EXPECT_NE(warning.find("recipe.build.configurations.options[0].unexpected"),
                  std::string::npos);
        EXPECT_NE(warning.find("[W]:"), std::string::npos);
    }

    /**
     * @brief Verifies that duplicate option names in a recipe are retained
     *        rather than deduplicated, and that a warning is emitted.
     *
     * Writes a recipe with two build options that share the name "feature".
     * The test asserts that:
     *   - Both entries are present in the parsed configuration (size == 2).
     *   - The captured warning contains the strings "duplicate option name
     *     'feature'" and "both entries are retained".
     *
     * This tests the parser's tolerance policy: duplicates are preserved
     * (lossless) but flagged so users can correct their recipes.
     */
    TEST_F(TemporaryDatabase, RetainsDuplicateOptionsAndEmitsWarning) {
        write("duplicate", "latest.yaml", R"(
recipe:
  name: duplicate
  type: package
  build:
    configurations:
      options:
        - name: feature
        - name: feature
)");

        testing::internal::CaptureStdout();
        PackageConfigPtr config   = get_db_config("duplicate", "latest");
        const std::string warning = testing::internal::GetCapturedStdout();

        ASSERT_TRUE(config->build.has_value());
        ASSERT_TRUE(config->build->configurations.has_value());
        EXPECT_EQ(config->build->configurations->options.size(), 2U);
        EXPECT_NE(warning.find("duplicate option name 'feature'"), std::string::npos);
        EXPECT_NE(warning.find("both entries are retained"), std::string::npos);
    }

    /**
     * @brief Verifies that overlapping version-range config files cause an
     *        immediate termination with a non-zero exit code.
     *
     * Creates three recipe files for "demo":
     *   - latest.yaml
     *   - 1.0-2.0.yaml
     *   - 2.0-3.0.yaml (shares the boundary 2.0 with the previous range)
     *
     * Requesting version 2.0 triggers the overlap check, and the test expects
     * the program to call exit(EXIT_FAILURE) with a message containing
     * "Overlapping database config ranges". This guards against ambiguous
     * recipe resolution when version intervals are not disjoint.
     */
    TEST_F(TemporaryDatabase, RejectsOverlappingConfigRanges) {
        const std::string config = "recipe: {name: demo, type: system}\n";
        write("demo", "latest.yaml", config);
        write("demo", "1.0-2.0.yaml", config);
        write("demo", "2.0-3.0.yaml", config);

        EXPECT_EXIT(static_cast<void>(get_db_config("demo", "2.0")),
                    ::testing::ExitedWithCode(EXIT_FAILURE), "Overlapping database config ranges");
    }

    /**
     * @brief Integration test that parses every recipe in the real database/
     *       directory and verifies SCOTCH version resolution.
     *
     * Iterates over every subdirectory of KEZ_SOURCE_DIR/database, requesting
     * the "latest" version of each package. Any parse failure causes the test
     * to report the failing directory path.
     *
     * Additionally, requests the SCOTCH package at version 6.1.3 and asserts:
     *   - The resolved name is "scotch6" (the version-range config for that
     *     version).
     *   - It was parsed as a MakePackageConfig.
     *
     * The database cache is cleared before and after the loop to ensure a
     * clean slate and to validate that cache re-initialisation works.
     */
    TEST(DatabaseIntegration, ParsesEveryRepositoryConfig) {
        const std::filesystem::path database = std::filesystem::path(KEZ_SOURCE_DIR) / "database";
        setenv("KEZ_DB", database.c_str(), 1);
        clear_db_cache();

        for (const auto& entry : std::filesystem::directory_iterator(database)) {
            if (!entry.is_directory()) {
                continue;
            }
            EXPECT_NO_THROW(
                static_cast<void>(get_db_config(entry.path().filename().string(), "latest")))
                << entry.path();
        }

        PackageConfigPtr scotch6 = get_db_config("scotch", "6.1.3");
        EXPECT_EQ(scotch6->name, "scotch6");
        EXPECT_NE(dynamic_cast<const MakePackageConfig*>(scotch6.get()), nullptr);
        clear_db_cache();
    }

}  // namespace
