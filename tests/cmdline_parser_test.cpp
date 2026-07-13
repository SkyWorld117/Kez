/**
 * @file    cmdline_parser_test.cpp
 * @brief   Unit tests for the command-line parser and install-plan executor.
 *
 * This test suite exercises the following components:
 *   - apply_cmdline_config():  Applies dot-notation overrides to a parsed
 *                               YAML configuration tree.
 *   - write_install_plan():    Serialises a BashCommandPlan into a shell
 *                               script install plan.
 *   - install.sh (executor):   Executes the generated plan, supporting
 *                               skip/force semantics, parallel dependency-
 *                               aware execution, and per-package logging.
 *
 * Every test creates an isolated temporary directory (keyed on PID to avoid
 * collisions) and cleans up after itself.  Because the executor tests invoke
 * the real install.sh script, they act as integration tests for both the
 * C++ plan-writer and the bash executor.
 */

#include <gtest/gtest.h>
#include <unistd.h>

#include <cmdline_parser/cmdline_parser.hpp>
#include <filesystem>
#include <string>
#include <utils/bash_utils.hpp>
#include <utils/file_utils.hpp>

namespace {

    std::string install_plan_package_block(const std::string& plan, const std::string& package) {
        const std::string begin = "kez_plan_begin " + package + "\n";
        const std::size_t start = plan.find(begin);
        if (start == std::string::npos) {
            return {};
        }
        const std::size_t end = plan.find("kez_plan_end\n", start);
        if (end == std::string::npos) {
            return {};
        }
        return plan.substr(start, end + std::string("kez_plan_end\n").size() - start);
    }

    /**
 * @test AppliesMapIndexAndNamedSequenceOverrides
 *
 * @brief Verify that apply_cmdline_config() can target arbitrary nodes in a
 *        YAML tree using both numeric index and named-key accessor syntax.
 *
 * The function receives a vector of dot-notation path=value strings and must
 * apply each override to the correct location in the YAML node hierarchy.
 *
 * Path variants exercised:
 *   - Top-level scalar override (application.version)
 *   - Deeply nested scalar inside a sequence element accessed by numeric
 *     index (build.configurations.environment.0.value)
 *   - A scalar inside a sequence element accessed by a sibling's `name` key
 *     (build.configurations.options.feature.enabled — where "feature" is the
 *     value of the `name` field of that sequence element, not an index)
 *   - The same named-accessor syntax inside a nested stage configuration
 *     (stages.0.configurations.options.threads.enabled_value)
 *
 * Edge cases:  Mixing index-based and name-based access in the same override
 * path ensures the parser correctly distinguishes numeric keys from named
 * lookups and does not inadvertently flatten the YAML structure.
 */
    TEST(CommandLineParser, AppliesMapIndexAndNamedSequenceOverrides) {
        YAML::Node config = YAML::Load(R"(
kez:
  application:
    version: 1.0
    build:
      configurations:
        environment:
          - name: CFLAGS
            value: -O2
        options:
          - name: feature
            enabled: false
      stages:
        - target: build
          configurations:
            options:
              - name: threads
                enabled_value: 4
)");

        apply_cmdline_config(
            config, {"application.version=2.0",
                     "application.build.configurations.environment.CFLAGS.value=-O3",
                     "application.build.configurations.options.feature.enabled=true",
                     "application.build.stages.build.configurations.options.0.enabled_value=16"});

        EXPECT_EQ(config["kez"]["application"]["version"].Scalar(), "2.0");
        EXPECT_EQ(config["kez"]["application"]["build"]["configurations"]["environment"][0]["value"]
                      .Scalar(),
                  "-O3");
        EXPECT_EQ(config["kez"]["application"]["build"]["configurations"]["options"][0]["enabled"]
                      .Scalar(),
                  "true");
        EXPECT_EQ(config["kez"]["application"]["build"]["stages"][0]["configurations"]["options"][0]
                        ["enabled_value"]
                            .Scalar(),
                  "16");
    }

    TEST(CommandLineParser, WritesShellQuotedExecutorPlan) {
        const std::filesystem::path directory =
            std::filesystem::temp_directory_path() /
            ("kez-commandline-test-" + std::to_string(getpid()));
        const std::filesystem::path path = directory / "plan.sh";
        std::filesystem::remove_all(directory);

        write_install_plan(
            {{"dependency", {"true"}, {}},
             {"package", {"printf '%s\\n' \"hello world\"", "cd source"}, {"dependency"}}},
            path);

        const std::string plan = read_file(path.string());
        EXPECT_EQ(plan.rfind("# kez-install-plan-v1\n", 0), 0U);
        EXPECT_NE(plan.find("kez_plan_begin 'dependency'"), std::string::npos);
        EXPECT_NE(plan.find("kez_plan_begin 'package'"), std::string::npos);
        EXPECT_NE(plan.find("kez_plan_depends 'dependency'"), std::string::npos);
        EXPECT_NE(plan.find("kez_plan_command 'printf '\\''%s\\n'\\'' \"hello world\"'"),
                  std::string::npos);
        EXPECT_NE(plan.find("kez_plan_end"), std::string::npos);

        std::filesystem::remove_all(directory);
    }

    TEST(CommandLineParser, BashExecutorTracksSkipAndForceState) {
        const std::filesystem::path directory =
            std::filesystem::temp_directory_path() /
            ("kez-install-executor-test-" + std::to_string(getpid()));
        const std::filesystem::path target  = directory / "target env";
        const std::filesystem::path plan    = directory / "plan.sh";
        const std::filesystem::path counter = directory / "counter file";
        std::filesystem::remove_all(directory);

        write_install_plan(
            {{"test-package", {"printf 'x\\n' >> " + shell_single_quote(counter.string())}, {}}},
            plan);
        const std::string executor =
            "env -u KEZ_HOME -u KEZ_WORKDIR bash " +
            shell_single_quote(std::string(KEZ_SOURCE_DIR) + "/scripts/install.sh") + " " +
            shell_single_quote(target.string()) + " " + shell_single_quote(plan.string());

        ASSERT_EQ(std::system(executor.c_str()), 0);
        EXPECT_EQ(read_file(counter.string()), "x\n");
        ASSERT_EQ(std::system(executor.c_str()), 0);
        EXPECT_EQ(read_file(counter.string()), "x\n");
        ASSERT_EQ(std::system((executor + " --force").c_str()), 0);
        EXPECT_EQ(read_file(counter.string()), "x\nx\n");
        EXPECT_EQ(read_file((target / "state.yaml").string()), "state:\n  - test-package\n");

        std::filesystem::remove_all(directory);
    }

    TEST(CommandLineParser, BashExecutorTracksVersionMapState) {
        const std::filesystem::path directory =
            std::filesystem::temp_directory_path() /
            ("kez-install-map-state-test-" + std::to_string(getpid()));
        const std::filesystem::path target  = directory / "system";
        const std::filesystem::path plan    = directory / "plan.sh";
        const std::filesystem::path counter = directory / "counter";
        std::filesystem::remove_all(directory);

        write_install_plan({{"test-package",
                             {"printf 'x\\n' >> " + shell_single_quote(counter.string()),
                              "printf '1.2.3\\n' > \"$KEZ_PACKAGE_VERSION_FILE\""},
                             {}}},
                           plan);
        const std::string executor =
            "KEZ_INSTALL_STATE_FORMAT=map env -u KEZ_HOME -u KEZ_WORKDIR bash " +
            shell_single_quote(std::string(KEZ_SOURCE_DIR) + "/scripts/install.sh") + " " +
            shell_single_quote(target.string()) + " " + shell_single_quote(plan.string());

        ASSERT_EQ(std::system(executor.c_str()), 0);
        ASSERT_EQ(std::system(executor.c_str()), 0);
        EXPECT_EQ(read_file(counter.string()), "x\n");
        EXPECT_EQ(read_file((target / "state.yaml").string()), "state:\n  test-package: 1.2.3\n");

        std::filesystem::remove_all(directory);
    }

    TEST(CommandLineParser, InitPlanEnforcesBootstrapCompilerDependencies) {
        const std::filesystem::path directory = std::filesystem::temp_directory_path() /
                                                ("kez-init-plan-test-" + std::to_string(getpid()));
        const std::filesystem::path plan      = directory / "plan.sh";
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(directory);

        const std::string generate =
            "KEZ_HOME=" + shell_single_quote(KEZ_SOURCE_DIR) + " bash -c " +
            shell_single_quote("source \"$KEZ_HOME/scripts/init.sh\"; write_init_plan \"$1\" 0") +
            " bash " + shell_single_quote(plan.string());
        ASSERT_EQ(std::system(generate.c_str()), 0);

        const std::string contents = read_file(plan.string());
        EXPECT_EQ(contents.rfind("# kez-install-plan-v1\n", 0), 0U);

        const std::string binutils = install_plan_package_block(contents, "binutils");
        EXPECT_NE(binutils.find("kez_plan_depends gmp\n"), std::string::npos);
        EXPECT_NE(binutils.find("kez_plan_depends zstd\n"), std::string::npos);
        const std::string gcc = install_plan_package_block(contents, "gcc");
        EXPECT_NE(gcc.find("kez_plan_depends binutils\n"), std::string::npos);

        for (const std::string& package : {"elfutils", "m4", "autoconf", "automake", "libtool",
                                           "make", "perl", "git", "yaml-cpp", "googletest"}) {
            const std::string block = install_plan_package_block(contents, package);
            ASSERT_FALSE(block.empty()) << package;
            EXPECT_NE(block.find("kez_plan_depends gcc\n"), std::string::npos) << package;
        }

        for (const std::string& package : {"cmake", "rust", "patchelf"}) {
            const std::string block = install_plan_package_block(contents, package);
            ASSERT_FALSE(block.empty()) << package;
            EXPECT_EQ(block.find("kez_plan_depends gcc\n"), std::string::npos) << package;
        }

        std::filesystem::remove_all(directory);
    }

    TEST(CommandLineParser, BashExecutorRunsReadyPackagesAndWritesLogs) {
        const std::filesystem::path directory =
            std::filesystem::temp_directory_path() /
            ("kez-install-parallel-test-" + std::to_string(getpid()));
        const std::filesystem::path target = directory / "target env";
        const std::filesystem::path plan   = directory / "plan.sh";
        std::filesystem::remove_all(directory);

        const std::filesystem::path base_done  = directory / "base.done";
        const std::filesystem::path left_done  = directory / "left.done";
        const std::filesystem::path right_done = directory / "right.done";
        const std::filesystem::path top_done   = directory / "top.done";

        write_install_plan(
            {{"base",
              {"printf 'base output\\n'", "touch " + shell_single_quote(base_done.string())},
              {}},
             {"left",
              {"test -f " + shell_single_quote(base_done.string()), "printf 'left output\\n'",
               "touch " + shell_single_quote(left_done.string())},
              {"base"}},
             {"right",
              {"test -f " + shell_single_quote(base_done.string()), "printf 'right output\\n'",
               "touch " + shell_single_quote(right_done.string())},
              {"base"}},
             {"top",
              {"test -f " + shell_single_quote(left_done.string()),
               "test -f " + shell_single_quote(right_done.string()),
               "touch " + shell_single_quote(top_done.string())},
              {"left", "right"}}},
            plan);
        const std::string executor =
            "KEZ_INSTALL_JOBS=2 env -u KEZ_HOME -u KEZ_WORKDIR bash " +
            shell_single_quote(std::string(KEZ_SOURCE_DIR) + "/scripts/install.sh") + " " +
            shell_single_quote(target.string()) + " " + shell_single_quote(plan.string());

        ASSERT_EQ(std::system(executor.c_str()), 0);
        EXPECT_TRUE(std::filesystem::exists(top_done));
        EXPECT_EQ(read_file((target / "state.yaml").string()),
                  "state:\n  - base\n  - left\n  - right\n  - top\n");
        EXPECT_NE(read_file((target / "logs" / "base.log").string()).find("base output"),
                  std::string::npos);
        EXPECT_NE(read_file((target / "logs" / "left.log").string()).find("left output"),
                  std::string::npos);
        EXPECT_NE(read_file((target / "logs" / "right.log").string()).find("right output"),
                  std::string::npos);

        std::filesystem::remove_all(directory);
    }

    TEST(CommandLineParser, BashExecutorReportsFailureLog) {
        const std::filesystem::path directory =
            std::filesystem::temp_directory_path() /
            ("kez-install-failure-test-" + std::to_string(getpid()));
        const std::filesystem::path target = directory / "target env";
        const std::filesystem::path plan   = directory / "plan.sh";
        const std::filesystem::path output = directory / "executor output";
        std::filesystem::remove_all(directory);

        write_install_plan({{"bad", {"printf 'hidden output\\n'", "printf 'boom\\n'; false"}, {}}},
                           plan);
        const std::string executor =
            "KEZ_INSTALL_JOBS=2 env -u KEZ_HOME -u KEZ_WORKDIR bash " +
            shell_single_quote(std::string(KEZ_SOURCE_DIR) + "/scripts/install.sh") + " " +
            shell_single_quote(target.string()) + " " + shell_single_quote(plan.string()) + " > " +
            shell_single_quote(output.string()) + " 2>&1";

        EXPECT_NE(std::system(executor.c_str()), 0);
        const std::string console = read_file(output.string());
        EXPECT_NE(console.find("Read log file:"), std::string::npos);
        EXPECT_EQ(console.find("boom"), std::string::npos);

        const std::string log = read_file((target / "logs" / "bad.log").string());
        EXPECT_NE(log.find("hidden output"), std::string::npos);
        EXPECT_NE(log.find("boom"), std::string::npos);
        EXPECT_NE(log.find("failed with exit status"), std::string::npos);

        std::filesystem::remove_all(directory);
    }

}  // namespace
