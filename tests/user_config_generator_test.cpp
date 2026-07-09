/**
 * @file    user_config_generator_test.cpp
 * @brief   Unit tests for the user-config generation pipeline.
 *
 * Tests cover the individual filter components (`configurations_filter`,
 * `options_filter`, `stages_filter`) as well as the top-level
 * `gen_user_config` entry point.  The test fixture `TemporaryGeneratorDatabase`
 * sets up an ephemeral KEZ environment with a minimal database, heuristics
 * tree, and manifest so that tests can exercise real YAML parsing and
 * filtering.
 */

#include <gtest/gtest.h>
#include <unistd.h>

#include <cstdlib>
#include <database/database.hpp>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <unordered_set>
#include <user_config_generator/configurations_filter.hpp>
#include <user_config_generator/options_filter.hpp>
#include <user_config_generator/stages_filter.hpp>
#include <user_config_generator/user_config_generator.hpp>
#include <vector>

namespace {

    /**
     * Test fixture that creates a temporary KEZ environment for user-config
     * generation tests.
     *
     * On SetUp the fixture:
     *   - Saves the current `KEZ_DB`, `KEZ_HOME`, `KEZ_WORKDIR`, and
     *     `KEZ_ARCH` environment variables for later restoration.
     *   - Creates a temporary directory with subdirectories for `database`,
     *     `heuristics`, `mpis`, `patches/<pkg>`, and `vendors`.
     *   - Sets those environment variables to point into the temp directory
     *     and forces `KEZ_ARCH` to `x86_64`.
     *   - Writes a minimal `manifest.yaml`.
     *   - Clears the database cache so that tests start from a clean slate.
     *
     * On TearDown it removes the temporary directory and restores the original
     * environment variables.
     *
     * Helper methods `write_package` and `write_file` let derived code quickly
     * populate the temporary database and filesystem.
     */
    class TemporaryGeneratorDatabase : public ::testing::Test {
       protected:
        void SetUp() override {
            remember_environment("KEZ_DB", previous_database_);
            remember_environment("KEZ_HOME", previous_home_);
            remember_environment("KEZ_WORKDIR", previous_work_directory_);
            remember_environment("KEZ_ARCH", previous_architecture_);

            path_ = std::filesystem::temp_directory_path() /
                    ("kez-user-config-generator-test-" + std::to_string(getpid()));
            std::filesystem::remove_all(path_);
            std::filesystem::create_directories(path_ / "database");
            std::filesystem::create_directories(path_ / "heuristics");
            std::filesystem::create_directories(path_ / "mpis");
            std::filesystem::create_directories(path_ / "patches" / "application");
            std::filesystem::create_directories(path_ / "vendors");
            setenv("KEZ_DB", (path_ / "database").c_str(), 1);
            setenv("KEZ_HOME", path_.c_str(), 1);
            setenv("KEZ_WORKDIR", path_.c_str(), 1);
            setenv("KEZ_ARCH", "x86_64", 1);
            write_file(path_ / "manifest.yaml", R"(
paths:
  mpis: mpis
  vendors: vendors
)");
            clear_db_cache();
        }

        void TearDown() override {
            clear_db_cache();
            std::filesystem::remove_all(path_);
            restore_environment("KEZ_DB", previous_database_);
            restore_environment("KEZ_HOME", previous_home_);
            restore_environment("KEZ_WORKDIR", previous_work_directory_);
            restore_environment("KEZ_ARCH", previous_architecture_);
        }

        /**
         * Saves the current value of an environment variable into an
         * optional<string> so it can be restored later.
         *
         * If the variable is not set the optional remains empty, which signals
         * to @ref restore_environment that it should unset the variable rather
         * than overwrite it.
         *
         * @param name     Environment variable name (e.g. "KEZ_DB").
         * @param previous Output parameter receiving the saved value (or
         *                 nullopt).
         */
        static void remember_environment(const char* name, std::optional<std::string>& previous) {
            const char* value = std::getenv(name);
            if (value != nullptr) {
                previous = value;
            }
        }

        /**
         * Restores an environment variable to a previously saved value.
         *
         * If @p previous has a value the variable is set to that value;
         * otherwise it is unset.  This lets the fixture cleanly undo its
         * SetUp modifications without leaking state across tests.
         *
         * @param name     Environment variable name.
         * @param previous The value saved earlier by @ref remember_environment.
         */
        static void restore_environment(const char* name,
                                        const std::optional<std::string>& previous) {
            if (previous.has_value()) {
                setenv(name, previous->c_str(), 1);
            } else {
                unsetenv(name);
            }
        }

        /**
         * Creates a package recipe at `database/<package>/latest.yaml` inside
         * the temporary environment.
         *
         * @param package  Package name (used as the directory name).
         * @param contents Raw YAML content of the recipe (the entire
         *                 `latest.yaml` file).
         */
        void write_package(const std::string& package, const std::string& contents) const {
            const std::filesystem::path directory = path_ / "database" / package;
            std::filesystem::create_directories(directory);
            std::ofstream output(directory / "latest.yaml");
            ASSERT_TRUE(output.good());
            output << contents;
        }

        /**
         * Writes arbitrary content to a file rooted in the temporary
         * environment path.
         *
         * @param path     Relative (or absolute) path; for typical use a
         *                 path relative to @ref path_ (e.g.
         *                 `path_ / "config.yaml"`).
         * @param contents File content to write.
         */
        void write_file(const std::filesystem::path& path, const std::string& contents) const {
            std::ofstream output(path);
            ASSERT_TRUE(output.good());
            output << contents;
        }

        std::filesystem::path path_;
        std::optional<std::string> previous_database_;
        std::optional<std::string> previous_home_;
        std::optional<std::string> previous_work_directory_;
        std::optional<std::string> previous_architecture_;
    };

    /**
     * Converts a YAML sequence node (of strings) into an unordered_set for
     * order-independent comparison.
     *
     * @param  sequence A YAML node that should represent a
     *                  `std::vector<std::string>`.
     * @return An unordered_set containing the same string values.
     */
    std::unordered_set<std::string> as_set(const YAML::Node& sequence) {
        const std::vector<std::string> values = sequence.as<std::vector<std::string>>();
        return {values.begin(), values.end()};
    }

    /**
     * Searches a YAML sequence of option nodes for one whose `"name"` field
     * matches @p name.
     *
     * @param  options Sequence of YAML nodes, each expected to have a
     *                 `"name"` string key.
     * @param  name    The option name to look for.
     * @return The matching YAML::Node if found.
     *
     * @note If no matching option exists the test is failed via ADD_FAILURE
     *       and an empty (null) node is returned.
     */
    YAML::Node find_option(const YAML::Node& options, const std::string& name) {
        for (YAML::Node option : options) {
            if (option["name"].as<std::string>() == name) {
                return option;
            }
        }
        ADD_FAILURE() << "Missing option: " << name;
        return YAML::Node();
    }

    /**
     * Checks whether a YAML sequence of option nodes contains one with a
     * given name.
     *
     * @param  options Sequence of YAML nodes, each expected to have a
     *                 `"name"` string key.
     * @param  name    The option name to look for.
     * @return true if a matching option exists, false otherwise.
     */
    bool has_option(const YAML::Node& options, const std::string& name) {
        for (YAML::Node option : options) {
            if (option["name"].as<std::string>() == name) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Verifies that `configurations_filter` and `stages_filter`
     *        correctly convert typed user-configurable fields and resolve
     *        abstract requirements.
     *
     * The test exercises the following scenarios:
     *   - An environment variable whose `requires` is satisfied by a concrete
     *     dependency appears in the filtered output with its default value
     *     preserved.
     *   - An environment variable whose `requires` is **not** satisfied is
     *     excluded from the filtered output.
     *   - A user-configurable build option with an `enabled` default of
     *     `false`, explicit `enabled_value`, and `disabled_format` is
     *     correctly carried through; its `disabled_value` field is null
     *     because the recipe did not set one.
     *   - A user-configurable option with no explicit defaults is present
     *     and enabled=true (the default), with a null `enabled_value`.
     *   - A non-user-configurable (`user_configurable: false`) option is
     *     excluded from the output.
     *   - `filtered_stages` preserves stage-level configurations and applies
     *     the same filtering to the stage's nested configurations.
     */
    TEST(UserConfigFilters, ConvertTypedUserConfigurableFieldsAndResolveAbstractRequirements) {
        BuildConfiguration configuration;

        EnvironmentVariable environment;
        environment.name              = "MPICC";
        environment.description       = "MPI C compiler";
        environment.user_configurable = true;
        environment.
            requires
        = {"mpi"};
        environment.value.default_value = "${mpi.c}";
        configuration.environment.push_back(environment);

        EnvironmentVariable unavailable;
        unavailable.name              = "MISSING";
        unavailable.user_configurable = true;
        unavailable.
            requires
        = {"not-selected"};
        unavailable.value.default_value = "unused";
        configuration.environment.push_back(unavailable);

        BuildOption feature;
        feature.name                         = "feature";
        feature.description                  = "Optional feature";
        feature.user_configurable            = true;
        feature.enabled                      = ConfigurableValue<bool> {};
        feature.enabled->default_value       = false;
        feature.enabled_value                = ConfigurableValue<std::string> {};
        feature.enabled_value->default_value = "enabled-value";
        feature.disabled_format              = "--without-feature";
        feature.
            requires
        = {"mpi"};
        configuration.options.push_back(feature);

        BuildOption default_enabled;
        default_enabled.name              = "default-enabled";
        default_enabled.user_configurable = true;
        configuration.options.push_back(default_enabled);

        BuildOption hidden;
        hidden.name              = "hidden";
        hidden.user_configurable = false;
        configuration.options.push_back(hidden);

        const std::vector<std::string> dependencies       = {"openmpi"};
        const AbstractPackageSelections abstract_packages = {{"mpi", "openmpi"}};
        const YAML::Node result =
            filtered_configurations(configuration, dependencies, abstract_packages);

        ASSERT_EQ(result["environment"].size(), 1U);
        EXPECT_EQ(result["environment"][0]["name"].as<std::string>(), "MPICC");
        EXPECT_EQ(result["environment"][0]["value"].as<std::string>(), "${mpi.c}");
        EXPECT_EQ(result["environment"][0]["requires"].as<std::vector<std::string>>(),
                  std::vector<std::string>({"mpi"}));

        ASSERT_EQ(result["options"].size(), 2U);
        const YAML::Node feature_output = result["options"][0];
        EXPECT_FALSE(feature_output["enabled"].as<bool>());
        EXPECT_EQ(feature_output["enabled_value"].as<std::string>(), "enabled-value");
        EXPECT_TRUE(feature_output["disabled_value"].IsNull());
        EXPECT_EQ(feature_output["requires"].as<std::vector<std::string>>(),
                  std::vector<std::string>({"mpi"}));

        const YAML::Node default_output = result["options"][1];
        EXPECT_TRUE(default_output["enabled"].as<bool>());
        EXPECT_TRUE(default_output["enabled_value"].IsNull());

        BuildStage stage;
        stage.target            = std::nullopt;
        stage.configurations    = configuration;
        const YAML::Node stages = filtered_stages({stage}, dependencies, abstract_packages);
        ASSERT_EQ(stages.size(), 1U);
        EXPECT_TRUE(stages[0]["target"].IsNull());
        EXPECT_EQ(stages[0]["configurations"]["options"].size(), 2U);
    }

    /**
     * @brief Full integration test: generates a user YAML from a set of typed
     *        package recipes and validates every section of the output.
     *
     * Packages created:
     *   - `application` (type: package) — declares source tarball releases,
     *     dependencies, build configurations (environment + options), and
     *     stage-level configurations.
     *   - `library` (type: package) — a git-sourced dependency.
     *   - `system-library` (type: system) — no build steps.
     *   - `external-tool` (type: external) — externally managed tool.
     *
     * Patch files are written to `patches/application/` to verify alphabetical
     * ordering and default-disabled state.
     *
     * Assertions cover:
     *   - The dependency list contains all transitive packages.
     *   - The recipe `targets` lists the requested target only.
     *   - Abstract packages map is empty (no abstract types involved).
     *   - System-typed packages are absent from the `kez` map.
     *   - The application entry has the correct description, the latest
     *     version (2.0.0), the compiler from `config.yaml`, alphabetically
     *     ordered patches (disabled by default), user-configurable
     *     environment and options with proper defaults (including
     *     enabled_value and disabled_value), and stage-level configurations
     *     that override global ones.
     *   - A library dependency is listed with its version and compiler.
     *   - An external-tool dependency has no compiler field.
     */
    TEST_F(TemporaryGeneratorDatabase, GeneratesUserYamlFromResolvedTypedPackageData) {
        write_file(path_ / "config.yaml", R"(
settings:
  default_compiler: gcc@13.4.0
)");
        write_file(path_ / "patches" / "application" / "z-last.patch", "last");
        write_file(path_ / "patches" / "application" / "a-first.patch", "first");

        write_package("application", R"(
recipe:
  name: application
  description: Test application
  type: package
  source:
    type: tarball
    releases:
      - version: 2.0.0
        url: https://example.invalid/application-2.0.0.tar.gz
      - version: 1.0.0
        url: https://example.invalid/application-1.0.0.tar.gz
  dependencies: [library, system-library, external-tool]
  build:
    configurations:
      environment:
        - name: CFLAGS
          description: Compiler flags
          user_configurable: true
          requires: [library]
          default: -O3
      options:
        - name: feature
          description: Build the feature
          user_configurable: true
          enabled:
            default: false
          enabled_value:
            default: enabled-value
          disabled_format: --without-feature
          disabled_value:
            default: disabled-value
          requires: [library]
    stages:
      - target: build
        configurations:
          environment:
            - name: STAGE_FLAGS
              user_configurable: true
              default: -g
)");
        write_package("library", R"(
recipe:
  name: library
  description: Test library
  type: package
  source:
    type: git
    url: https://example.invalid/library.git
    releases:
      - version: main
        tag: main
)");
        write_package("system-library", R"(
recipe:
  name: system-library
  type: system
)");
        write_package("external-tool", R"(
recipe:
  name: external-tool
  description: Externally managed tool
  type: external
)");

        const YAML::Node result = gen_user_config({"application"}, false);

        EXPECT_EQ(as_set(result["recipe"]["dependencies"]),
                  std::unordered_set<std::string>(
                      {"application", "library", "system-library", "external-tool"}));
        EXPECT_EQ(result["recipe"]["targets"].as<std::vector<std::string>>(),
                  std::vector<std::string>({"application"}));
        EXPECT_TRUE(result["recipe"]["abstract_packages"].IsMap());
        EXPECT_EQ(result["recipe"]["abstract_packages"].size(), 0U);

        ASSERT_EQ(result["kez"].size(), 3U);
        EXPECT_FALSE(result["kez"]["system-library"].IsDefined());

        const YAML::Node application = result["kez"]["application"];
        EXPECT_EQ(application["description"].as<std::string>(), "Test application");
        EXPECT_EQ(application["version"].as<std::string>(), "2.0.0");
        EXPECT_EQ(application["compiler"].as<std::string>(), "gcc@13.4.0");
        ASSERT_EQ(application["patches"].size(), 2U);
        EXPECT_EQ(application["patches"][0]["name"].as<std::string>(), "a-first.patch");
        EXPECT_FALSE(application["patches"][0]["enabled"].as<bool>());
        EXPECT_EQ(application["patches"][1]["name"].as<std::string>(), "z-last.patch");

        const YAML::Node configurations = application["build"]["configurations"];
        ASSERT_EQ(configurations["environment"].size(), 1U);
        EXPECT_EQ(configurations["environment"][0]["value"].as<std::string>(), "-O3");
        ASSERT_EQ(configurations["options"].size(), 1U);
        EXPECT_FALSE(configurations["options"][0]["enabled"].as<bool>());
        EXPECT_EQ(configurations["options"][0]["enabled_value"].as<std::string>(), "enabled-value");
        EXPECT_EQ(configurations["options"][0]["disabled_value"].as<std::string>(),
                  "disabled-value");

        ASSERT_EQ(application["build"]["stages"].size(), 1U);
        EXPECT_EQ(application["build"]["stages"][0]["target"].as<std::string>(), "build");
        EXPECT_EQ(application["build"]["stages"][0]["configurations"]["environment"][0]["value"]
                      .as<std::string>(),
                  "-g");

        EXPECT_EQ(result["kez"]["library"]["version"].as<std::string>(), "main");
        EXPECT_EQ(result["kez"]["library"]["compiler"].as<std::string>(), "gcc@13.4.0");
        EXPECT_FALSE(result["kez"]["external-tool"]["compiler"].IsDefined());
    }

    /**
     * @brief Verifies that packages declaring a `toolchain` (cmake, autotools)
     *        automatically surface toolchain-specific build options as editable
     *        templates in the generated user config.
     *
     * For each toolchain the test checks:
     *   - The expected well-known options exist (e.g. `CMAKE_INSTALL_PREFIX`,
     *     `prefix`, `CFLAGS`, `LDFLAGS`, `CMAKE_C_FLAGS`,
     *     `CMAKE_EXE_LINKER_FLAGS`).
     *   - Each option's `enabled_value` is a template string referencing
     *     either the package's own prefix or properties from dependencies
     *     (e.g. `${library.includes}`, `${library.libs}`).
     *   - Options that should be omitted when no system dependency provides
     *     them (`CMAKE_PREFIX_PATH`, `LIBS`) are absent.
     *
     * Edge cases:
     *   - Inter-package property references (`${pkg.property}`) are emitted
     *     as literal template strings, not resolved.
     *   - Packages with no explicit `build.configurations` (empty `{}`)
     *     still produce toolchain defaults.
     *   - Both CMake and Autotools paths are exercised independently.
     */
    TEST_F(TemporaryGeneratorDatabase, ExposesToolchainGeneratedOptionsAsEditableTemplates) {
        write_package("gcc", R"(
recipe:
  name: gcc
  type: compiler
  properties:
    c: /usr/bin/gcc
    cxx: /usr/bin/g++
    fort: /usr/bin/gfortran
)");
        write_package("library", R"(
recipe:
  name: library
  type: package
  properties:
    include: ${library.prefix}/include
    lib: ${library.prefix}/lib
    libs: -lexample
)");
        write_package("cmake-application", R"(
recipe:
  name: cmake-application
  type: package
  toolchain: cmake
  dependencies: [library]
  build:
    configurations: {}
)");
        write_package("autotools-application", R"(
recipe:
  name: autotools-application
  type: package
  toolchain: autotools
  dependencies: [library]
  build:
    configurations: {}
)");

        const YAML::Node cmake = gen_user_config({"cmake-application"}, false, "system");
        const YAML::Node cmake_options =
            cmake["kez"]["cmake-application"]["build"]["configurations"]["options"];
        EXPECT_EQ(
            find_option(cmake_options, "CMAKE_INSTALL_PREFIX")["enabled_value"].as<std::string>(),
            "${cmake-application.prefix}");
        EXPECT_EQ(find_option(cmake_options, "CMAKE_C_FLAGS")["enabled_value"].as<std::string>(),
                  "-O3 ${library.includes}");
        EXPECT_EQ(
            find_option(cmake_options, "CMAKE_EXE_LINKER_FLAGS")["enabled_value"].as<std::string>(),
            "${library.ldflags} ${library.libs}");
        EXPECT_FALSE(has_option(cmake_options, "CMAKE_PREFIX_PATH"));

        const YAML::Node autotools = gen_user_config({"autotools-application"}, false, "system");
        const YAML::Node autotools_options =
            autotools["kez"]["autotools-application"]["build"]["configurations"]["options"];
        EXPECT_EQ(find_option(autotools_options, "prefix")["enabled_value"].as<std::string>(),
                  "${autotools-application.prefix}");
        EXPECT_EQ(find_option(autotools_options, "CFLAGS")["enabled_value"].as<std::string>(),
                  "-O3 ${library.includes}");
        EXPECT_EQ(find_option(autotools_options, "LDFLAGS")["enabled_value"].as<std::string>(),
                  "${library.ldflags}");
        EXPECT_FALSE(has_option(autotools_options, "LIBS"));
    }

    /**
     * @brief Verifies that abstract package types are resolved to their
     *        architecture-appropriate concrete implementation and that the
     *        selection is recorded in the generated user config.
     *
     * The test sets up:
     *   - A `heuristics/advice.yaml` mapping the abstract `mpi` type to
     *     `implementation` on `x86_64`.
     *   - An abstract `mpi` package that lists `implementation` as an
     *     available implementation.
     *   - A concrete `implementation` package with a tarball source and
     *     user-configurable options.
     *
     * Expectations:
     *   - `recipe.abstract_packages.mpi` records `"implementation"`.
     *   - `recipe.dependencies` lists the concrete package, not `mpi`.
     *   - `recipe.targets` still lists the original abstract target (`mpi`).
     *   - The `kez` map contains the concrete package entry with its version
     *     and build configuration.
     */
    TEST_F(TemporaryGeneratorDatabase, RecordsAbstractSelectionAndConfiguresItsConcreteTarget) {
        write_file(path_ / "heuristics" / "advice.yaml", R"(
advice:
  mpi:
    x86_64: implementation
)");
        write_package("mpi", R"(
recipe:
  name: mpi
  type: abstract
  implementations: [implementation]
)");
        write_package("implementation", R"(
recipe:
  name: implementation
  type: mpi
  source:
    type: tarball
    releases:
      - version: 5.0.0
        url: https://example.invalid/implementation-5.0.0.tar.gz
  build:
    configurations:
      options:
        - name: feature
          user_configurable: true
          enabled:
            default: true
)");
        std::filesystem::remove_all(path_ / "mpis");

        testing::internal::CaptureStdout();
        const YAML::Node result = gen_user_config({"mpi"}, false, "system");
        testing::internal::GetCapturedStdout();

        EXPECT_EQ(result["recipe"]["abstract_packages"]["mpi"].as<std::string>(), "implementation");
        EXPECT_EQ(result["recipe"]["dependencies"].as<std::vector<std::string>>(),
                  std::vector<std::string>({"implementation"}));
        EXPECT_EQ(result["recipe"]["targets"].as<std::vector<std::string>>(),
                  std::vector<std::string>({"mpi"}));
        EXPECT_EQ(result["kez"]["implementation"]["version"].as<std::string>(), "5.0.0");
        EXPECT_EQ(result["kez"]["implementation"]["build"]["configurations"]["options"].size(), 1U);
    }

    /**
     * @brief Verifies that `gen_user_config` selects the latest installed
     *        version when a package already exists in the `mpis/` or
     *        `vendors/` directories.
     *
     * Edge cases:
     *   - Multiple installed versions of an MPI implementation
     *     (`implementation-4.1.0-system` and `implementation-5.2.0-system`);
     *     the highest version string (`5.2.0`) is picked.
     *   - Multiple installed versions of a vendor tool
     *     (`vendor-tool-2024.2` and `vendor-tool-2025.3`); the highest
     *     version (`2025.3`) is picked.
     *   - The database recipe's declared `releases` may differ from what is
     *     installed; the installed version takes precedence.
     */
    TEST_F(TemporaryGeneratorDatabase, UsesLatestInstalledMpiAndVendorVersions) {
        write_package("implementation", R"(
recipe:
  name: implementation
  type: mpi
  source:
    type: tarball
    releases:
      - version: 5.0.0
        url: https://example.invalid/implementation-5.0.0.tar.gz
)");
        std::filesystem::create_directories(path_ / "mpis" / "implementation-4.1.0-system");
        std::filesystem::create_directories(path_ / "mpis" / "implementation-5.2.0-system");

        const YAML::Node mpi = gen_user_config({"implementation"}, false, "system");
        EXPECT_EQ(mpi["kez"]["implementation"]["version"].as<std::string>(), "5.2.0");

        write_package("vendor-tool", R"(
recipe:
  name: vendor-tool
  type: vendor
  source:
    type: tarball
    releases:
      - version: 2025.1
        url: https://example.invalid/vendor-tool-2025.1.tar.gz
)");
        std::filesystem::create_directories(path_ / "vendors" / "vendor-tool-2024.2");
        std::filesystem::create_directories(path_ / "vendors" / "vendor-tool-2025.3");

        const YAML::Node vendor = gen_user_config({"vendor-tool"}, false, "system");
        EXPECT_EQ(vendor["kez"]["vendor-tool"]["version"].as<std::string>(), "2025.3");
    }

}  // namespace
