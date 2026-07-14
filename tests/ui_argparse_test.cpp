/**
 * @file ui_argparse_test.cpp
 * @brief Unit tests for non-terminating UI argument parsing.
 */

#include <gtest/gtest.h>

#include <string>
#include <ui/argparse.hpp>
#include <vector>

namespace {

    TEST(UiArgparse, ParsesTopLevelCommands) {
        EXPECT_EQ(parse_ui_arguments({}).command, UiCommand::Help);
        EXPECT_EQ(parse_ui_arguments({"--help"}).command, UiCommand::Help);
        EXPECT_EQ(parse_ui_arguments({"--version"}).command, UiCommand::Version);

        const UiArgumentsParseResult install = parse_ui_arguments({"install", "hdf5", "--dry-run"});
        EXPECT_TRUE(install.error.empty());
        EXPECT_EQ(install.command, UiCommand::Install);
        EXPECT_EQ(install.arguments, (CommandArguments {"hdf5", "--dry-run"}));
        EXPECT_EQ(parse_ui_arguments({"unknown"}).error, "Unknown command: unknown");
    }

    TEST(UiArgparse, ParsesInitAndUpdateOptions) {
        const InitOptionsParseResult init =
            parse_init_options({"--refresh", "--use-distro-compiler"});
        EXPECT_TRUE(init.error.empty());
        EXPECT_TRUE(init.options.refresh);
        EXPECT_TRUE(init.options.use_distro_compiler);
        EXPECT_TRUE(parse_init_options({"--help", "--bad"}).help);
        EXPECT_EQ(parse_init_options({"--bad"}).error, "Unknown init option: --bad");

        EXPECT_TRUE(parse_update_options({"--with-system"}).options.with_system);
        EXPECT_TRUE(parse_update_options({"--help", "--bad"}).help);
        EXPECT_EQ(parse_update_options({"--bad"}).error, "Unknown update option: --bad");
    }

    TEST(UiArgparse, ParsesInstallAndUtilitiesArguments) {
        const InstallOptionsParseResult install =
            parse_install_options({"--read=config.yaml", "-d", "-f", "-S", "-e", "research", "-c",
                                   "a=1", "b=2", "--", "--literal"},
                                  false);
        EXPECT_TRUE(install.error.empty());
        EXPECT_TRUE(install.options.read_file);
        EXPECT_TRUE(install.options.dry_run);
        EXPECT_TRUE(install.options.force);
        EXPECT_TRUE(install.options.with_slurm);
        EXPECT_EQ(install.options.environment, "research");
        EXPECT_EQ(install.options.overrides, (std::vector<std::string> {"a=1", "b=2"}));
        EXPECT_EQ(install.options.positional,
                  (std::vector<std::string> {"config.yaml", "--literal"}));
        EXPECT_EQ(parse_install_options({"--env", "name"}, true).error,
                  "--env is not valid for utility installation");
        EXPECT_EQ(parse_install_options({"--rebuild"}, false).error, "Missing value for --rebuild");

        const UtilitiesArgumentsParseResult add =
            parse_utilities_arguments({"add", "hdf5", "--force"});
        EXPECT_EQ(add.arguments.action, UtilitiesAction::Add);
        EXPECT_EQ(add.arguments.install_arguments, (CommandArguments {"hdf5", "--force"}));
        EXPECT_EQ(parse_utilities_arguments({"remove"}).error,
                  "utilities remove requires exactly one package name");
        EXPECT_EQ(parse_utilities_arguments({"unknown"}).error,
                  "Unknown utilities command: unknown");
    }

    TEST(UiArgparse, ParsesPackageCommands) {
        const UconfOptionsParseResult uconf =
            parse_uconf_options({"hdf5", "--save", "config.yaml", "netcdf"});
        EXPECT_TRUE(uconf.error.empty());
        EXPECT_TRUE(uconf.options.interactive);
        EXPECT_EQ(uconf.options.output_path, "config.yaml");
        EXPECT_EQ(uconf.options.packages, (std::vector<std::string> {"hdf5", "netcdf"}));
        EXPECT_EQ(parse_uconf_options({"--save"}).error, "Missing value for --save");

        const InfoArgumentsParseResult info = parse_info_arguments({"hdf5", "--raw"});
        EXPECT_TRUE(info.error.empty());
        EXPECT_EQ(info.arguments.package, "hdf5");
        EXPECT_TRUE(info.arguments.raw);
        EXPECT_EQ(parse_info_arguments({"hdf5", "--bad"}).error,
                  "Usage: kez info <package> [--raw]");

        EXPECT_TRUE(parse_dbcheck_options({}).all_packages);
        EXPECT_TRUE(parse_dbcheck_options({"--help"}).help);
        EXPECT_EQ(parse_dbcheck_options({"--only", "hdf5", "netcdf"}).packages,
                  (std::vector<std::string> {"hdf5", "netcdf"}));
        EXPECT_EQ(parse_dbcheck_options({"--only"}).error,
                  "--only requires at least one package name");
    }

    TEST(UiArgparse, ParsesEnvironmentCommands) {
        const EnvironmentArgumentsParseResult create =
            parse_environment_arguments({"create", "research"});
        EXPECT_TRUE(create.error.empty());
        EXPECT_EQ(create.arguments.action, EnvironmentAction::Create);
        EXPECT_EQ(create.arguments.name, "research");
        EXPECT_EQ(parse_environment_arguments({"list", "extra"}).error,
                  "env list does not accept additional arguments");
        EXPECT_EQ(parse_environment_arguments({"activate"}).error,
                  "env activate requires exactly one name");
        EXPECT_EQ(parse_environment_arguments({"unknown"}).error, "Unknown env command: unknown");

        const ManagedEnvironmentArgumentsParseResult load =
            parse_managed_environment_arguments({"load", "gcc-14"}, "compiler");
        EXPECT_TRUE(load.error.empty());
        EXPECT_EQ(load.arguments.action, ManagedEnvironmentAction::Load);
        EXPECT_EQ(load.arguments.name, "gcc-14");
        EXPECT_EQ(parse_managed_environment_arguments({"unload", "extra"}, "mpi").error,
                  "mpi unload does not accept additional arguments");
    }

    TEST(UiArgparse, ParsesFactoryCommandsAndBuildOptions) {
        const FactoryArgumentsParseResult build =
            parse_factory_arguments({"build", "-d", "--force", "-S"});
        EXPECT_TRUE(build.error.empty());
        EXPECT_EQ(build.arguments.action, FactoryAction::Build);
        EXPECT_TRUE(build.arguments.build_options.dry_run);
        EXPECT_TRUE(build.arguments.build_options.force);
        EXPECT_TRUE(build.arguments.build_options.with_slurm);

        const FactoryArgumentsParseResult help =
            parse_factory_arguments({"build", "--help", "--bad"});
        EXPECT_EQ(help.arguments.action, FactoryAction::Help);
        EXPECT_TRUE(help.error.empty());
        EXPECT_EQ(parse_factory_arguments({"create"}).error,
                  "factory create requires exactly one name");
        EXPECT_EQ(parse_factory_arguments({"build", "--bad"}).error,
                  "Unknown factory build option: --bad");
    }

}  // namespace
