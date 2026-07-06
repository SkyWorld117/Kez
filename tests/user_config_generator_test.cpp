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

        static void remember_environment(const char* name, std::optional<std::string>& previous) {
            const char* value = std::getenv(name);
            if (value != nullptr) {
                previous = value;
            }
        }

        static void restore_environment(const char* name,
                                        const std::optional<std::string>& previous) {
            if (previous.has_value()) {
                setenv(name, previous->c_str(), 1);
            } else {
                unsetenv(name);
            }
        }

        void write_package(const std::string& package, const std::string& contents) const {
            const std::filesystem::path directory = path_ / "database" / package;
            std::filesystem::create_directories(directory);
            std::ofstream output(directory / "latest.yaml");
            ASSERT_TRUE(output.good());
            output << contents;
        }

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

    std::unordered_set<std::string> as_set(const YAML::Node& sequence) {
        const std::vector<std::string> values = sequence.as<std::vector<std::string>>();
        return {values.begin(), values.end()};
    }

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
