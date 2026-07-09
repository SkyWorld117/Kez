/**
 * @file bash_completion_test.cpp
 * @brief Unit tests for the bash-completion suggestion engine.
 *
 * Verifies that `completion_suggestions` returns correct candidates for
 * partial command lines, respecting filesystem state, managed environments,
 * and single-use option semantics.
 */

#include <gtest/gtest.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <ui/bash_completion.hpp>
#include <vector>

namespace {
    /**
     * @brief Test fixture that sets up a temporary, isolated completion
     *        environment.
     *
     * On SetUp the fixture:
     *   - Saves the current values of KEZ_HOME, KEZ_WORKDIR, and KEZ_DB so they
     *     can be restored in TearDown.
     *   - Creates a temporary directory tree under the system temp directory
     *     with a minimal database (alpha, beta), a work area containing managed
     *     environments (research, gcc-14, openmpi-5-gcc-14), and a factory
     *     (sweep).
     *   - Writes a manifest.yaml that maps environment categories to their
     *     subdirectories, and a dummy recipe.yaml.
     *   - Overrides the three environment variables to point into the
     *     temporary tree and changes the current working directory into it.
     *
     * On TearDown the fixture restores the original environment variables,
     * the original working directory, and removes the temporary tree.
     */
    class TemporaryCompletionEnvironment : public ::testing::Test {
       protected:
        void SetUp() override {
            remember("KEZ_HOME", previous_home_);
            remember("KEZ_WORKDIR", previous_work_);
            remember("KEZ_DB", previous_database_);
            previous_directory_ = std::filesystem::current_path();

            root_ = std::filesystem::temp_directory_path() /
                    ("kez-completion-test-" + std::to_string(getpid()));
            std::filesystem::remove_all(root_);
            std::filesystem::create_directories(root_ / "database" / "alpha");
            std::filesystem::create_directories(root_ / "database" / "beta");
            std::filesystem::create_directories(root_ / "work" / "env" / "applications" /
                                                "research");
            std::filesystem::create_directories(root_ / "work" / "env" / "compilers" / "gcc-14");
            std::filesystem::create_directories(root_ / "work" / "env" / "mpis" /
                                                "openmpi-5-gcc-14");
            std::filesystem::create_directories(root_ / "work" / "factories" / "sweep");

            write(root_ / "manifest.yaml", R"(
paths:
  applications: env/applications
  compilers: env/compilers
  mpis: env/mpis
  factories: factories
)");
            write(root_ / "recipe.yaml", "recipe: {}\n");
            setenv("KEZ_HOME", root_.c_str(), 1);
            setenv("KEZ_WORKDIR", (root_ / "work").c_str(), 1);
            setenv("KEZ_DB", (root_ / "database").c_str(), 1);
            std::filesystem::current_path(root_);
        }

        void TearDown() override {
            std::filesystem::current_path(previous_directory_);
            restore("KEZ_HOME", previous_home_);
            restore("KEZ_WORKDIR", previous_work_);
            restore("KEZ_DB", previous_database_);
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
            std::ofstream output(path);
            ASSERT_TRUE(output.good());
            output << contents;
        }

        static bool has(const std::vector<std::string>& values, const std::string& value) {
            return std::find(values.begin(), values.end(), value) != values.end();
        }

        std::filesystem::path root_;
        std::filesystem::path previous_directory_;
        std::optional<std::string> previous_home_;
        std::optional<std::string> previous_work_;
        std::optional<std::string> previous_database_;
    };

    TEST_F(TemporaryCompletionEnvironment, FiltersTopLevelAndSuggestsPackagesAndOptions) {
        EXPECT_EQ(completion_suggestions(1, {"kez", "in"}),
                  std::vector<std::string>({"info", "init", "install"}));

        const std::vector<std::string> install = completion_suggestions(2, {"kez", "install", ""});
        EXPECT_TRUE(has(install, "alpha"));
        EXPECT_TRUE(has(install, "beta"));
        EXPECT_TRUE(has(install, "--env"));
        EXPECT_TRUE(has(install, "--read"));
        EXPECT_TRUE(has(install, "--with-slurm"));
    }

    TEST_F(TemporaryCompletionEnvironment, UsesContextForFilesystemAndManagedEnvironments) {
        EXPECT_EQ(completion_suggestions(3, {"kez", "install", "--env", ""}),
                  std::vector<std::string>({"research"}));

        const std::vector<std::string> files =
            completion_suggestions(3, {"kez", "install", "--read", ""});
        EXPECT_TRUE(has(files, "recipe.yaml"));

        const std::vector<std::string> compilers =
            completion_suggestions(3, {"kez", "compiler", "load", ""});
        EXPECT_TRUE(has(compilers, "gcc-14"));
        const std::vector<std::string> mpis = completion_suggestions(3, {"kez", "mpi", "load", ""});
        EXPECT_TRUE(has(mpis, "openmpi-5-gcc-14"));

        const std::vector<std::string> factories =
            completion_suggestions(3, {"kez", "factory", "enter", ""});
        EXPECT_TRUE(has(factories, "sweep"));
    }

    TEST_F(TemporaryCompletionEnvironment, DoesNotRepeatSingleUseOptions) {
        const std::vector<std::string> suggestions =
            completion_suggestions(4, {"kez", "install", "alpha", "--force", ""});
        EXPECT_FALSE(has(suggestions, "--force"));
        EXPECT_FALSE(has(suggestions, "-f"));
        EXPECT_TRUE(has(suggestions, "--config"));
    }
}  // namespace
