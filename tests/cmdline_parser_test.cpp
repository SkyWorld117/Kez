#include <gtest/gtest.h>
#include <unistd.h>

#include <cmdline_parser/cmdline_parser.hpp>
#include <filesystem>
#include <string>
#include <utils/bash_utils.hpp>
#include <utils/file_utils.hpp>

namespace {
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
