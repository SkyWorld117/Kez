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

        /**
         * @brief Save an environment variable's current value into an
         *        optional string, if it is set.
         *
         * @param name  The environment variable name to snapshot.
         * @param value Output parameter; receives the current value if set,
         *              otherwise is left empty (nullopt).
         */
        static void remember(const char* name, std::optional<std::string>& value) {
            const char* current = std::getenv(name);
            if (current != nullptr) {
                value = current;
            }
        }

        /**
         * @brief Restore an environment variable to a previously saved value.
         *
         * If the saved value is present it is set; otherwise the variable is
         * unset.
         *
         * @param name  The environment variable name to restore.
         * @param value The optional previously-saved value (from remember()).
         */
        static void restore(const char* name, const std::optional<std::string>& value) {
            if (value.has_value()) {
                setenv(name, value->c_str(), 1);
            } else {
                unsetenv(name);
            }
        }

        /**
         * @brief Create or overwrite a file with the given contents.
         *
         * The test is aborted immediately if the file cannot be opened.
         *
         * @param path     Filesystem path of the file to write.
         * @param contents The string content to write into the file.
         */
        static void write(const std::filesystem::path& path, const std::string& contents) {
            std::ofstream output(path);
            ASSERT_TRUE(output.good());
            output << contents;
        }

        /**
         * @brief Check whether a sorted vector contains a specific value.
         *
         * Uses binary search via std::find (the vector need not be sorted).
         *
         * @param values The vector to search.
         * @param value  The string to look for.
         * @return true if value appears in values, false otherwise.
         */
        static bool has(const std::vector<std::string>& values, const std::string& value) {
            return std::find(values.begin(), values.end(), value) != values.end();
        }

        std::filesystem::path root_;
        std::filesystem::path previous_directory_;
        std::optional<std::string> previous_home_;
        std::optional<std::string> previous_work_;
        std::optional<std::string> previous_database_;
    };

    /**
     * @test Verifies that the completion engine suggests matching top-level
     *       subcommands and, for the `install` command, correctly proposes
     *       both package names and global options.
     *
     * Edge cases covered:
     *   - Partial subcommand prefix ("in") is expanded to all matching
     *     commands ("info", "init", "install").
     *   - A trailing empty argument after "install" causes the engine to
     *     return package names from the database (alpha, beta) interleaved
     *     with valid global flags (--env, --read, --with-slurm).
     */
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

    /**
     * @test Verifies that the completion engine uses the current command
     *       context to produce filesystem-aware and environment-aware
     *       suggestions.
     *
     * Edge cases covered:
     *   - `install --env ` suggests managed application environments
     *     ("research") drawn from the manifest's applications path.
     *   - `install --read ` falls back to filesystem completion (lists
     *     files in the current directory, e.g. "recipe.yaml").
     *   - `compiler load ` suggests known compilers ("gcc-14") from the
     *     compilers subdirectory.
     *   - `mpi load ` suggests known MPI stacks ("openmpi-5-gcc-14") from
     *     the mpis subdirectory.
     *   - `factory enter ` suggests known factory profiles ("sweep") from
     *     the factories subdirectory.
     */
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

    /**
     * @test Verifies that single-use options are not suggested a second time
     *       when they have already appeared on the command line.
     *
     * Edge cases covered:
     *   - After `--force` (or its short form `-f`) is already present in the
     *       token list, neither form reappears in the suggestions.
     *   - A different option (`--config`) that has not yet been used is
     *       still offered.
     */
    TEST_F(TemporaryCompletionEnvironment, DoesNotRepeatSingleUseOptions) {
        const std::vector<std::string> suggestions =
            completion_suggestions(4, {"kez", "install", "alpha", "--force", ""});
        EXPECT_FALSE(has(suggestions, "--force"));
        EXPECT_FALSE(has(suggestions, "-f"));
        EXPECT_TRUE(has(suggestions, "--config"));
    }
}  // namespace
