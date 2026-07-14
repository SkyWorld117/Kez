/**
 * @file ui_commands_test.cpp
 * @brief Unit tests for pure helpers kept beside their owning UI commands.
 */

#include <gtest/gtest.h>
#include <yaml-cpp/yaml.h>

#include <database/config.hpp>
#include <filesystem>
#include <regex>
#include <string>
#include <ui/factory.hpp>
#include <ui/install.hpp>
#include <ui/packages.hpp>
#include <vector>

namespace {

    TEST(InstallCommand, BuildsExecutorCommandsAndSelectsRebuildPlans) {
        const std::string direct = install_executor_command(
            "/opt/kez home/install.sh", "/tmp/env's", "/tmp/plan.sh", true, false, "kez-install");
        EXPECT_NE(direct.find("'/opt/kez home/install.sh'"), std::string::npos);
        EXPECT_NE(direct.find("'/tmp/env'\\''s'"), std::string::npos);
        EXPECT_NE(direct.find(" --force"), std::string::npos);

        const std::string slurm = install_executor_command(
            "/opt/install.sh", "/tmp/env", "/tmp/plan.sh", false, true, "factory-job");
        EXPECT_EQ(slurm.rfind("sbatch --wait --job-name=factory-job --wrap=", 0), 0U);

        const BashCommandPlan plan          = {{"application", {"build app"}, {"library"}},
                                               {"tool", {"build tool"}, {"library"}},
                                               {"library", {"build library"}, {}}};
        const RebuildPlanSelection selected = select_rebuild_plan(plan, "library");
        EXPECT_TRUE(selected.target_found);
        EXPECT_EQ(selected.packages, (std::vector<std::string> {"application", "tool", "library"}));
        ASSERT_EQ(selected.plan.size(), 3U);

        const RebuildPlanSelection missing = select_rebuild_plan(plan, "missing");
        EXPECT_FALSE(missing.target_found);
        EXPECT_TRUE(missing.packages.empty());
        EXPECT_TRUE(missing.plan.empty());
    }

    TEST(InstallCommand, RemovesPackagesFromStateWithoutMutatingInput) {
        const YAML::Node state   = YAML::Load("[alpha, remove-me, {invalid: entry}, beta]");
        const YAML::Node updated = state_without_package(state, "remove-me");

        ASSERT_EQ(updated.size(), 2U);
        EXPECT_EQ(updated[0].Scalar(), "alpha");
        EXPECT_EQ(updated[1].Scalar(), "beta");
        EXPECT_EQ(state.size(), 4U);
    }

    TEST(FactoryCommand, RendersScriptsAndMatchesSummaries) {
        FactoryProfile profile;
        profile.name     = "benchmark";
        profile.commands = {"prepare input", "run benchmark > kez.out"};
        const std::string script =
            render_factory_profile_script("/tmp/run space", "/tmp/build space", profile);
        EXPECT_EQ(script.rfind("#!/usr/bin/env bash\nset -Eeuo pipefail\n", 0), 0U);
        EXPECT_NE(script.find("cd '/tmp/run space'"), std::string::npos);
        EXPECT_NE(script.find("kez_factory_info 'Executing: prepare input'"), std::string::npos);
        EXPECT_NE(script.find("run benchmark > kez.out"), std::string::npos);

        EXPECT_EQ(matching_factory_summary_lines("time=4\nnoise\ntime=2\n", std::regex("^time=")),
                  (std::vector<std::string> {"time=4", "time=2"}));
    }

    TEST(PackageCommand, ExtractsDisplayMetadataFromTypedConfiguration) {
        GenericPackageConfig package;
        Build build;
        BuildConfiguration top;
        BuildOption option;
        option.name              = "shared";
        option.description       = "Build shared libraries";
        option.user_configurable = true;
        option.enabled_value     = ConfigurableValue<std::string> {"on", {}};
        top.options.push_back(option);

        EnvironmentVariable environment;
        environment.name              = "CFLAGS";
        environment.description       = "Compiler flags";
        environment.user_configurable = true;
        top.environment.push_back(environment);
        build.configurations = top;

        BuildStage stage;
        stage.configurations = BuildConfiguration {};
        BuildOption stage_option;
        stage_option.name              = "threads";
        stage_option.user_configurable = true;
        stage.configurations->options.push_back(stage_option);
        build.stages.push_back(stage);
        package.build = build;

        const std::vector<PackageConfigEntry> entries = package_config_entries(package);
        ASSERT_EQ(entries.size(), 3U);
        EXPECT_EQ(entries[0].name, "shared");
        ASSERT_TRUE(entries[0].default_value.has_value());
        EXPECT_EQ(*entries[0].default_value, "on");
        EXPECT_EQ(entries[1].name, "CFLAGS");
        EXPECT_EQ(entries[2].name, "threads");

        EXPECT_EQ(toolchain_name(Toolchain::CMake), "cmake");
        EXPECT_EQ(source_type_name(SourceType::Tarball), "tarball");
        EXPECT_EQ(property_display_value(Property {"prefix", std::string("/opt/demo")}),
                  "/opt/demo");
        EXPECT_EQ(property_display_value(
                      Property {"conditional", ConfigurableValue<std::string> {std::nullopt, {}}}),
                  "<conditional>");
    }

}  // namespace
