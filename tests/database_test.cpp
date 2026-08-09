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

        std::filesystem::path path_;     ///< Path to the temporary database root.
        std::string previous_database_;  ///< Saved KEZ_DB value, empty if none was set.
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
        EXPECT_EQ(range_end->name, range_start->name);
        EXPECT_NE(dynamic_cast<const AutotoolsPackageConfig*>(range_start.get()), nullptr);
        EXPECT_EQ(range_start->default_configuration_command(), "./configure");
        EXPECT_EQ(latest->name, "demo");
        EXPECT_NE(dynamic_cast<const CMakePackageConfig*>(latest.get()), nullptr);
        EXPECT_EQ(latest->default_configuration_command(), "cmake -B build");
        EXPECT_EQ(latest->default_stage_command(BuildStage {"install", true, std::nullopt}, 4),
                  "cmake --install build");
        EXPECT_EQ(get_db_config("demo")->name, latest->name);
    }

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
        ASSERT_EQ(config->dependencies.size(), 1U);
    }

    TEST_F(TemporaryDatabase, ParsesPythonToolchainAndAddsInterpreterDependency) {
        write("demo", "latest.yaml", R"(
recipe:
  name: demo
  type: package
  toolchain: python
  source:
    type: pypi
    releases:
      - version: 1.2.3
)");

        PackageConfigPtr config = get_db_config("demo");
        ASSERT_NE(dynamic_cast<const PythonPackageConfig*>(config.get()), nullptr);
        EXPECT_EQ(config->toolchain(), Toolchain::Python);
        ASSERT_TRUE(config->source.has_value());
        EXPECT_EQ(config->source->type, SourceType::PyPI);
        ASSERT_EQ(config->source->releases.size(), 1U);
        EXPECT_EQ(config->source->releases.front().version, "1.2.3");
        ASSERT_EQ(config->dependencies.size(), 1U);
        EXPECT_EQ(config->dependencies.front().name, "python");
    }

    TEST_F(TemporaryDatabase, RejectsPypiSourceWithoutPythonToolchain) {
        write("invalid", "latest.yaml", R"(
recipe:
  name: invalid
  type: package
  source:
    type: pypi
    releases:
      - version: 1.0.0
)");

        EXPECT_EXIT(static_cast<void>(get_db_config("invalid")),
                    ::testing::ExitedWithCode(EXIT_FAILURE),
                    "type 'pypi' requires toolchain 'python'");
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

    TEST(PackageConfig, ProvidesToolchainDefaultsAndPropertyAliases) {
        GenericPackageConfig generic;
        AutotoolsPackageConfig autotools;
        CMakePackageConfig cmake;
        MakePackageConfig make;
        PythonPackageConfig python;

        const BuildStage parallel {"all", true, std::nullopt};
        const BuildStage serial {"check", false, std::nullopt};
        const BuildStage untargeted {std::nullopt, true, std::nullopt};

        EXPECT_EQ(generic.toolchain(), Toolchain::None);
        EXPECT_EQ(generic.default_configuration_command(), std::nullopt);
        EXPECT_EQ(generic.default_stage_command(parallel, 8), std::nullopt);

        EXPECT_EQ(autotools.toolchain(), Toolchain::Autotools);
        EXPECT_EQ(autotools.default_configuration_command(), "./configure");
        EXPECT_EQ(autotools.default_stage_command(parallel, 8), "make -j8 all");
        EXPECT_EQ(autotools.default_stage_command(serial, 8), "make check");

        EXPECT_EQ(cmake.toolchain(), Toolchain::CMake);
        EXPECT_EQ(cmake.default_configuration_command(), "cmake -B build");
        EXPECT_EQ(cmake.default_stage_command(parallel, 8),
                  "cmake --build build --parallel 8 --target all");
        EXPECT_EQ(cmake.default_stage_command(serial, 8), "cmake --build build --target check");
        EXPECT_EQ(cmake.default_stage_command(BuildStage {"install", false, std::nullopt}, 8),
                  "cmake --install build");

        MesonPackageConfig meson;
        EXPECT_EQ(meson.toolchain(), Toolchain::Meson);
        EXPECT_EQ(meson.default_configuration_command(), "meson setup build");
        EXPECT_EQ(meson.default_stage_command(parallel, 8), "meson compile -C build -j8 all");
        EXPECT_EQ(meson.default_stage_command(serial, 8), "meson compile -C build check");
        EXPECT_EQ(meson.default_stage_command(BuildStage {"install", false, std::nullopt}, 8),
                  "meson install -C build");

        EXPECT_EQ(make.toolchain(), Toolchain::Make);
        EXPECT_EQ(make.default_configuration_command(), std::nullopt);
        EXPECT_EQ(make.default_stage_command(untargeted, 0), "make");

        EXPECT_EQ(python.toolchain(), Toolchain::Python);
        EXPECT_EQ(python.default_configuration_command(), std::nullopt);
        EXPECT_EQ(python.default_stage_command(parallel, 8), std::nullopt);

        generic.properties = {{"include", std::string("/include")},
                              {"lib", std::string("/lib")},
                              {"libs", std::string("-lkez")}};
        ASSERT_NE(find_property(generic, "libs"), nullptr);
        EXPECT_EQ(find_property(generic, "missing"), nullptr);
        EXPECT_TRUE(has_property(generic, "incflags"));
        EXPECT_TRUE(has_property(generic, "ldflags"));
        EXPECT_TRUE(has_property(generic, "libs"));
        EXPECT_FALSE(has_property(generic, "bin"));
    }

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
        EXPECT_EQ(scotch6->name, "scotch");
        EXPECT_NE(dynamic_cast<const MakePackageConfig*>(scotch6.get()), nullptr);
        clear_db_cache();
    }

}  // namespace

TEST_F(TemporaryDatabase, ResolvesVersionConstraintsFromRangeFiles) {
    write("libfoo", "latest.yaml", R"(
recipe:
  name: libfoo
  type: package
  source:
    type: git
    url: https://example.invalid/libfoo.git
    releases:
      - version: 2.0.0
        tag: v2.0.0
)");
    write("libfoo", "1.0.0-1.0.0.yaml", R"(
recipe:
  name: libfoo-legacy
  type: package
)");
    write("libfoo", "1.5.0-1.5.0.yaml", R"(
recipe:
  name: libfoo-middle
  type: package
)");

    // No constraints -> "latest"
    EXPECT_EQ(resolve_dependency_version("libfoo", {}), "latest");

    // Constraint < 2.0.0 should pick 1.5.0
    std::vector<DependencyConstraint> lt_2 = {{"<", "2.0.0"}};
    EXPECT_EQ(resolve_dependency_version("libfoo", lt_2), "1.5.0");

    // Constraint <= 1.0.0 should pick 1.0.0
    std::vector<DependencyConstraint> le_1 = {{"<=", "1.0.0"}};
    EXPECT_EQ(resolve_dependency_version("libfoo", le_1), "1.0.0");

    // Range 1.0.0 <= x < 2.0.0 should pick 1.5.0
    std::vector<DependencyConstraint> range = {{">=", "1.0.0"}, {"<", "2.0.0"}};
    EXPECT_EQ(resolve_dependency_version("libfoo", range), "1.5.0");

    // == 2.0.0 should pick 2.0.0 (from releases)
    std::vector<DependencyConstraint> eq_2 = {{"==", "2.0.0"}};
    EXPECT_EQ(resolve_dependency_version("libfoo", eq_2), "2.0.0");
}

TEST_F(TemporaryDatabase, FailsOnUnsatisfiableVersionConstraint) {
    write("libbar", "latest.yaml", R"(
recipe:
  name: libbar
  type: package
  source:
    type: git
    url: https://example.invalid/libbar.git
    releases:
      - version: 1.0.0
        tag: v1.0.0
)");

    std::vector<DependencyConstraint> impossible = {{">=", "2.0.0"}};
    EXPECT_EXIT(resolve_dependency_version("libbar", impossible),
                ::testing::ExitedWithCode(EXIT_FAILURE),
                "No available version of 'libbar' satisfies");
}

TEST_F(TemporaryDatabase, ParsesDependenciesWithVersionConstraints) {
    write("consumer", "latest.yaml", R"(
recipe:
  name: consumer
  type: package
  dependencies:
    - alib@>=1.0
    - blib
    - clib@>=2.0,<3.0
)");

    PackageConfigPtr config = get_db_config("consumer");
    ASSERT_EQ(config->dependencies.size(), 3U);

    // First dep: "alib" with constraint ">=1.0"
    EXPECT_EQ(config->dependencies[0].name, "alib");
    ASSERT_EQ(config->dependencies[0].constraints.size(), 1U);
    EXPECT_EQ(config->dependencies[0].constraints[0].op, ">=");
    EXPECT_EQ(config->dependencies[0].constraints[0].version, "1.0");

    // Second dep: "blib" with no constraints
    EXPECT_EQ(config->dependencies[1].name, "blib");
    EXPECT_TRUE(config->dependencies[1].constraints.empty());

    // Third dep: "clib" with combined constraints
    EXPECT_EQ(config->dependencies[2].name, "clib");
    ASSERT_EQ(config->dependencies[2].constraints.size(), 2U);
    EXPECT_EQ(config->dependencies[2].constraints[0].op, ">=");
    EXPECT_EQ(config->dependencies[2].constraints[0].version, "2.0");
    EXPECT_EQ(config->dependencies[2].constraints[1].op, "<");
    EXPECT_EQ(config->dependencies[2].constraints[1].version, "3.0");
}
