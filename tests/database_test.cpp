#include <gtest/gtest.h>
#include <unistd.h>

#include <cstdlib>
#include <database/database.hpp>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

    class TemporaryDatabase : public ::testing::Test {
       protected:
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

        void TearDown() override {
            clear_db_cache();
            std::filesystem::remove_all(path_);
            if (previous_database_.empty()) {
                unsetenv("KEZ_DB");
            } else {
                setenv("KEZ_DB", previous_database_.c_str(), 1);
            }
        }

        void write(const std::string& package, const std::string& filename,
                   const std::string& contents) const {
            const std::filesystem::path directory = path_ / package;
            std::filesystem::create_directories(directory);
            std::ofstream output(directory / filename);
            ASSERT_TRUE(output.good());
            output << contents;
        }

        std::filesystem::path path_;
        std::string previous_database_;
    };

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

    TEST_F(TemporaryDatabase, ConvertsEveryDocumentedSectionToTypedData) {
        write("typed", "latest.yaml", R"(
recipe:
  name: typed
  description: Typed package
  author: test
  type: package
  toolchain: makefile
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
        ASSERT_NE(dynamic_cast<const MakefilePackageConfig*>(config.get()), nullptr);
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

    TEST_F(TemporaryDatabase, RejectsOverlappingConfigRanges) {
        const std::string config = "recipe: {name: demo, type: system}\n";
        write("demo", "latest.yaml", config);
        write("demo", "1.0-2.0.yaml", config);
        write("demo", "2.0-3.0.yaml", config);

        EXPECT_EXIT(static_cast<void>(get_db_config("demo", "2.0")),
                    ::testing::ExitedWithCode(EXIT_FAILURE), "Overlapping database config ranges");
    }

    TEST(DatabaseIntegration, ParsesEveryRepositoryConfig) {
        const std::filesystem::path database = std::filesystem::path(KEZ_SOURCE_DIR) / "database";
        setenv("KEZ_DB", database.c_str(), 1);
        clear_db_cache();

        std::size_t package_count = 0;
        for (const auto& entry : std::filesystem::directory_iterator(database)) {
            if (!entry.is_directory()) {
                continue;
            }
            ++package_count;
            EXPECT_NO_THROW(
                static_cast<void>(get_db_config(entry.path().filename().string(), "latest")))
                << entry.path();
        }
        EXPECT_EQ(package_count, 119U);

        PackageConfigPtr scotch6 = get_db_config("scotch", "6.1.3");
        EXPECT_EQ(scotch6->name, "scotch6");
        EXPECT_NE(dynamic_cast<const MakefilePackageConfig*>(scotch6.get()), nullptr);
        clear_db_cache();
    }

}  // namespace
