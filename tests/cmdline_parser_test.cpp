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

    /**
 * @test WritesShellQuotedExecutorPlan
 *
 * @brief Verify that write_install_plan() produces a valid v1 shell plan
 *        with properly shell-quoted commands and dependency declarations.
 *
 * The test constructs a small BashCommandPlan containing two packages
 * (one of which depends on the other) and writes it to a temporary file.
 * It then reads the file back and checks for the expected structural
 * elements:
 *
 *   - The first line must be the version marker "# kez-install-plan-v1\n".
 *   - Each package block opens with "kez_plan_begin '<name>'" and closes
 *     with "kez_plan_end".
 *   - Dependencies are declared via "kez_plan_depends '<dep>'".
 *   - Each command is emitted as "kez_plan_command '<quoted-command>'" with
 *     proper single-quote escaping (e.g. a literal single quote inside a
 *     single-quoted string becomes '\'').
 *
 * Edge cases:  Commands containing spaces, single quotes, and special
 * characters (printf '%s\n' "hello world") exercise the shell-quoting
 * logic to ensure the emitted script is safe to eval.
 */
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

    /**
 * @test BashExecutorTracksSkipAndForceState
 *
 * @brief Verify that the bash executor (install.sh) correctly skips
 *        already-installed packages on a subsequent run and re-executes
 *        them when the --force flag is supplied.
 *
 * The test writes a plan with a single package whose command appends a
 * line to a counter file. It then runs the executor three times:
 *
 *   1. First invocation — the package runs, appending "x\n".
 *   2. Second invocation (no flags) — the package is already recorded in
 *      state.yaml, so the executor **skips** it and the counter file is
 *      unchanged.
 *   3. Third invocation with --force — the executor ignores the cached
 *      state and re-runs the package, appending another "x\n".
 *
 * The test also confirms that state.yaml is written to the target
 * environment directory and contains the installed package name.
 *
 * Edge cases:
 *   - Empty environment (no KEZ_HOME or KEZ_WORKDIR) ensures the executor
 *     does not depend on the user's shell environment.
 *   - The counter-file approach makes the test immune to idempotency
 *     assumptions (the command itself is not idempotent, so a skip is
 *     visibly different from a re-run).
 */
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

    /**
 * @test BashExecutorRunsReadyPackagesAndWritesLogs
 *
 * @brief Verify that the executor correctly handles a multi-package
 *        dependency graph with parallel execution and per-package logging.
 *
 * The plan defines four packages forming a two-level diamond dependency
 * graph:
 *
 *              base
 *             /    \
 *          left    right
 *             \    /
 *              top
 *
 * base has no dependencies; left and right depend on base; top depends on
 * both left and right.  The executor is launched with KEZ_INSTALL_JOBS=2,
 * so base and at least one of {left, right} should run concurrently.
 *
 * Each package touches a sentinel file only after all its prerequisites
 * are satisfied (left/right wait for base's sentinel; top waits for both
 * left's and right's sentinels).  This design lets the test verify
 * topological ordering without racing.
 *
 * Assertions:
 *   - The executor exits successfully (exit code 0).
 *   - All four sentinel files exist after execution.
 *   - state.yaml lists packages in the correct dependency order
 *     (base first, then left and right, then top).
 *   - Each package's stdout/stderr is captured in its own log file under
 *     <target>/logs/<name>.log.
 *
 * Edge cases:
 *   - Parallel execution of independent packages within the same stage.
 *   - Guaranteeing that state.yaml reflects the topological (install)
 *     order rather than the arbitrary order the packages finished in.
 *   - Log files are written to a structured directory under the target
 *     environment and contain the expected output for each package.
 */
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

    /**
 * @test BashExecutorReportsFailureLog
 *
 * @brief Verify that when a package fails, the executor prints the path to
 *        its log file on stderr rather than inlining the full error output.
 *
 * The plan contains a single package "bad" whose commands print a benign
 * message ("hidden output") and then deliberately fail (printf + false).
 * Both stdout and stderr of the executor are redirected to a capture file.
 *
 * Assertions:
 *   - The executor exits with a non-zero status.
 *   - The captured console output contains the string "Read log file:",
 *     indicating the executor directs the user to the persisted log for
 *     details.
 *   - The console output does **not** contain the failure details ("boom"
 *     text) — those are written only to the per-package log file, not to
 *     the terminal.
 *   - The per-package log file (<target>/logs/bad.log) contains both the
 *     ordinary output ("hidden output") and the error output ("boom"),
 *     plus an indication of the exit status.
 *
 * Edge cases:
 *   - Ensuring sensitive error details are not leaked to the terminal
 *     (only the log path is shown).
 *   - Verifying that the log file is still written even when the package
 *     fails early, so the user can inspect the full failure context.
 */
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
