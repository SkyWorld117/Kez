/**
 * @file ui_utils_test.cpp
 * @brief Unit tests for UI path, argument, and shell-environment helpers.
 */

#include <gtest/gtest.h>
#include <unistd.h>
#include <yaml-cpp/yaml.h>

#include <cstdlib>
#include <database/database.hpp>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <ui/ui_utils.hpp>
#include <vector>

namespace {

    class TemporaryUiEnvironment : public ::testing::Test {
       protected:
        void SetUp() override {
            remember("KEZ_HOME", previous_home_);
            remember("KEZ_WORKDIR", previous_work_);
            remember("KEZ_DB", previous_database_);
            remember("KEZ_ACTIVE_ENV", previous_active_environment_);

            const ::testing::TestInfo* info =
                ::testing::UnitTest::GetInstance()->current_test_info();
            root_ = std::filesystem::temp_directory_path() /
                    ("kez-ui-utils-test-" + std::to_string(getpid()) + "-" + info->name());
            std::filesystem::remove_all(root_);
            std::filesystem::create_directories(root_ / "database");
            std::filesystem::create_directories(root_ / "work");

            write(root_ / "manifest.yaml", "paths:\n"
                                           "  applications: env/applications\n"
                                           "  utilities: env/utilities\n"
                                           "  system: env/system\n"
                                           "  compilers: env/compilers\n"
                                           "  mpis: env/mpis\n"
                                           "  vendors: env/vendors\n"
                                           "  absolute: " +
                                               (root_ / "absolute").string() + "\n");
            setenv("KEZ_HOME", root_.c_str(), 1);
            setenv("KEZ_WORKDIR", (root_ / "work").c_str(), 1);
            setenv("KEZ_DB", (root_ / "database").c_str(), 1);
            unsetenv("KEZ_ACTIVE_ENV");
            clear_db_cache();
        }

        void TearDown() override {
            clear_db_cache();
            restore("KEZ_HOME", previous_home_);
            restore("KEZ_WORKDIR", previous_work_);
            restore("KEZ_DB", previous_database_);
            restore("KEZ_ACTIVE_ENV", previous_active_environment_);
            std::filesystem::remove_all(root_);
        }

        static void remember(const char* name, std::optional<std::string>& value) {
            const char* current = std::getenv(name);
            if (current != nullptr) {
                value = current;
            }
        }

        static void restore(const char* name, const std::optional<std::string>& value) {
            if (value.has_value()) {
                setenv(name, value->c_str(), 1);
            } else {
                unsetenv(name);
            }
        }

        static void write(const std::filesystem::path& path, const std::string& contents) {
            std::filesystem::create_directories(path.parent_path());
            std::ofstream output(path);
            ASSERT_TRUE(output.good());
            output << contents;
        }

        void write_package(const std::string& name, const std::string& type) const {
            write(root_ / "database" / name / "latest.yaml",
                  "recipe: {name: " + name + ", type: " + type + "}\n");
        }

        std::filesystem::path root_;
        std::optional<std::string> previous_home_;
        std::optional<std::string> previous_work_;
        std::optional<std::string> previous_database_;
        std::optional<std::string> previous_active_environment_;
    };

    TEST(UiUtils, NamesEveryPackageType) {
        EXPECT_EQ(package_type_name(PackageType::Package), "package");
        EXPECT_EQ(package_type_name(PackageType::System), "system");
        EXPECT_EQ(package_type_name(PackageType::Compiler), "compiler");
        EXPECT_EQ(package_type_name(PackageType::Mpi), "mpi");
        EXPECT_EQ(package_type_name(PackageType::Vendor), "vendor");
        EXPECT_EQ(package_type_name(PackageType::Abstract), "abstract");
        EXPECT_EQ(package_type_name(PackageType::External), "external");
        EXPECT_EQ(package_type_name(static_cast<PackageType>(999)), "unknown");
    }

    TEST(UiUtils, ExtractsTargetsAndValidatesPathComponents) {
        const YAML::Node config = YAML::Load("recipe: {targets: [hdf5, openmpi]}\n");
        EXPECT_EQ(user_config_targets(config), (std::vector<std::string> {"hdf5", "openmpi"}));
        EXPECT_NO_FATAL_FAILURE(validate_path_component("gcc-13.4", "compiler name"));

        EXPECT_EXIT(validate_path_component("../escape", "environment name"),
                    ::testing::ExitedWithCode(EXIT_FAILURE), "Invalid environment name");
    }

    TEST(UiUtils, RejectsMalformedTargetLists) {
        EXPECT_EXIT(static_cast<void>(user_config_targets(YAML::Load("recipe: {}\n"))),
                    ::testing::ExitedWithCode(EXIT_FAILURE), "recipe.targets must be a sequence");
        EXPECT_EXIT(static_cast<void>(user_config_targets(YAML::Load("recipe: {targets: []}\n"))),
                    ::testing::ExitedWithCode(EXIT_FAILURE), "recipe.targets must not be empty");
        EXPECT_EXIT(
            static_cast<void>(user_config_targets(YAML::Load("recipe: {targets: [[bad]]}\n"))),
            ::testing::ExitedWithCode(EXIT_FAILURE), "target package must be a scalar");
    }

    TEST_F(TemporaryUiEnvironment, ResolvesRelativeAndAbsoluteManifestPaths) {
        EXPECT_EQ(configured_work_path("applications"), root_ / "work" / "env/applications");
        EXPECT_EQ(configured_work_path("absolute"), root_ / "absolute");
        EXPECT_EXIT(static_cast<void>(configured_work_path("missing")),
                    ::testing::ExitedWithCode(EXIT_FAILURE), "paths.missing is missing");
    }

    TEST_F(TemporaryUiEnvironment, ComputesPrefixesForEachInstallablePackageType) {
        write_package("application", "package");
        write_package("second-app", "package");
        write_package("gcc", "compiler");
        write_package("openmpi", "mpi");
        write_package("cuda", "vendor");
        write_package("system-lib", "system");
        write_package("mpi-api", "abstract");

        const YAML::Node applications = YAML::Load(R"(
recipe: {targets: [application, second-app]}
kez: {}
)");
        EXPECT_EQ(installation_prefix(applications, "research", false),
                  root_ / "work/env/applications/research");
        EXPECT_EQ(installation_prefix(applications, "", true), root_ / "work/env/utilities");

        setenv("KEZ_ACTIVE_ENV", "active", 1);
        EXPECT_EQ(installation_prefix(applications, "", false),
                  root_ / "work/env/applications/active");

        const YAML::Node compiler = YAML::Load(R"(
recipe: {targets: [gcc]}
kez:
  gcc: {version: '13.4.0@/local/source'}
)");
        EXPECT_EQ(installation_prefix(compiler, "", false),
                  root_ / "work/env/compilers/gcc-13.4.0");

        const YAML::Node mpi = YAML::Load(R"(
recipe:
  targets: [mpi-api]
  abstract_packages: {mpi-api: openmpi}
kez:
  openmpi: {version: 5.0.6, compiler: 'gcc@13.4.0'}
)");
        EXPECT_EQ(installation_prefix(mpi, "", false),
                  root_ / "work/env/mpis/openmpi-5.0.6-gcc-13.4.0");

        const YAML::Node vendor = YAML::Load(R"(
recipe: {targets: [cuda]}
kez:
  cuda: {version: '12.8'}
)");
        EXPECT_EQ(installation_prefix(vendor, "", false), root_ / "work/env/vendors/cuda-12.8");

        const YAML::Node system = YAML::Load("recipe: {targets: [system-lib]}\nkez: {}\n");
        EXPECT_EQ(installation_prefix(system, "", false), root_ / "work/env/system");
    }

    TEST_F(TemporaryUiEnvironment, RejectsInvalidInstallationCombinations) {
        write_package("application", "package");
        write_package("gcc", "compiler");

        const YAML::Node mixed = YAML::Load(R"(
recipe: {targets: [application, gcc]}
kez:
  gcc: {version: '13.4.0'}
)");
        EXPECT_EXIT(static_cast<void>(installation_prefix(mixed, "env", false)),
                    ::testing::ExitedWithCode(EXIT_FAILURE), "must have the same package type");

        const YAML::Node application = YAML::Load("recipe: {targets: [application]}\nkez: {}\n");
        EXPECT_EXIT(static_cast<void>(installation_prefix(application, "../escape", false)),
                    ::testing::ExitedWithCode(EXIT_FAILURE), "Invalid environment name");
    }

    TEST_F(TemporaryUiEnvironment, ListsOnlyDirectoriesInSortedOrder) {
        const std::filesystem::path environments = root_ / "environments";
        std::filesystem::create_directories(environments / "zeta");
        std::filesystem::create_directories(environments / "alpha");
        write(environments / "not-a-directory", "ignored\n");

        testing::internal::CaptureStdout();
        list_directories(environments, "environments");
        const std::string output = testing::internal::GetCapturedStdout();
        ASSERT_NE(output.find("Available environments"), std::string::npos);
        ASSERT_NE(output.find("alpha"), std::string::npos);
        ASSERT_NE(output.find("zeta"), std::string::npos);
        EXPECT_LT(output.find("alpha"), output.find("zeta"));
        EXPECT_EQ(output.find("not-a-directory"), std::string::npos);

        testing::internal::CaptureStdout();
        list_directories(root_ / "missing", "environments");
        EXPECT_NE(testing::internal::GetCapturedStdout().find("No environments found"),
                  std::string::npos);
    }

    TEST_F(TemporaryUiEnvironment, EmitsActivationAndDeactivationCommandsForExistingPaths) {
        const std::filesystem::path prefix  = root_ / "environment";
        const std::filesystem::path package = prefix / "package with spaces";
        std::filesystem::create_directories(package / "bin");
        std::filesystem::create_directories(package / "share/man");
        std::filesystem::create_directories(package / "lib/pkgconfig");
        std::filesystem::create_directories(package / "lib64/pkgconfig");
        std::filesystem::create_directories(package / "share/pkgconfig");
        std::filesystem::create_directories(prefix / ".hidden/bin");

        testing::internal::CaptureStdout();
        emit_environment_activation(prefix, "KEZ_ACTIVE_ENV", "team's environment");
        const std::string activation = testing::internal::GetCapturedStdout();
        EXPECT_NE(activation.find((package / "bin").string()), std::string::npos);
        EXPECT_NE(activation.find((package / "share/man").string()), std::string::npos);
        EXPECT_NE(activation.find((package / "lib/pkgconfig").string()), std::string::npos);
        EXPECT_NE(activation.find((package / "lib64/pkgconfig").string()), std::string::npos);
        EXPECT_NE(activation.find((package / "share/pkgconfig").string()), std::string::npos);
        EXPECT_NE(activation.find("export KEZ_ACTIVE_ENV='team'\\''s environment'"),
                  std::string::npos);
        EXPECT_EQ(activation.find(".hidden"), std::string::npos);

        testing::internal::CaptureStdout();
        emit_environment_deactivation(prefix, "KEZ_ACTIVE_ENV");
        const std::string deactivation = testing::internal::GetCapturedStdout();
        EXPECT_NE(deactivation.find((package / "bin").string()), std::string::npos);
        EXPECT_NE(deactivation.find("export PATH=\"$kez_nw\""), std::string::npos);
        EXPECT_NE(deactivation.find("unset KEZ_ACTIVE_ENV"), std::string::npos);
        EXPECT_EQ(deactivation.find(".hidden"), std::string::npos);
    }

    TEST(UiUtils, RunsExternalCommandsAndPropagatesFailures) {
        EXPECT_NO_FATAL_FAILURE(run_external_command("exit 0"));
        EXPECT_EXIT(run_external_command("exit 7"), ::testing::ExitedWithCode(EXIT_FAILURE),
                    "Command failed");
    }

}  // namespace
