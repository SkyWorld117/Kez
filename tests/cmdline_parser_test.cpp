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
 *                               aware execution, per-package logging, and the
 *                               interactive command-progress display.
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
#include <fstream>
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

    void make_executable(const std::filesystem::path& path) {
        std::filesystem::permissions(path,
                                     std::filesystem::perms::owner_read |
                                         std::filesystem::perms::owner_write |
                                         std::filesystem::perms::owner_exec,
                                     std::filesystem::perm_options::replace);
    }

    void write_fake_python(const std::filesystem::path& path) {
        std::ofstream output(path);
        ASSERT_TRUE(output.good());
        output << R"(#!/usr/bin/env bash
set -eu
if [[ ${1:-} == -c ]]; then
    printf '3.12.0 cpython-312\n'
else
    exit 2
fi
)";
        output.close();
        make_executable(path);
    }

    void write_fake_uv(const std::filesystem::path& path) {
        std::ofstream output(path);
        ASSERT_TRUE(output.good());
        output << R"(#!/usr/bin/env bash
set -eu

command=${1:-}
shift || true
case $command in
    venv)
        base_python=
        virtual_environment=
        while (( $# != 0 )); do
            case $1 in
                --python)
                    base_python=$2
                    shift 2
                    ;;
                --no-config | --no-python-downloads) shift ;;
                *)
                    virtual_environment=$1
                    shift
                    ;;
            esac
        done
        mkdir -p "$virtual_environment/bin"
        mkdir -p "$virtual_environment/lib/python3.12/site-packages"
        cp "$base_python" "$virtual_environment/bin/python"
        chmod +x "$virtual_environment/bin/python"
        ;;
    pip)
        operation=${1:-}
        shift || true
        case $operation in
            compile)
                base_python=
                output_file=
                input_file=
                while (( $# != 0 )); do
                    case $1 in
                        --python)
                            base_python=$2
                            shift 2
                            ;;
                        --output-file)
                            output_file=$2
                            shift 2
                            ;;
                        --no-config | --no-python-downloads | --no-header) shift ;;
                        *)
                            input_file=$1
                            shift
                            ;;
                    esac
                done
                fake_env=$(cd "$(dirname "$base_python")/../.." && pwd)
                [[ ! -e $fake_env/fail-uv-compile ]] || exit 1
                cp "$input_file" "$output_file"
                : > "$fake_env/uv-compile-ran"
                ;;
            sync)
                python=
                lock_file=
                while (( $# != 0 )); do
                    case $1 in
                        --python)
                            python=$2
                            shift 2
                            ;;
                        --no-config | --no-python-downloads) shift ;;
                        *)
                            lock_file=$1
                            shift
                            ;;
                    esac
                done
                fake_env=$(cd "$(dirname "$python")/../.." && pwd)
                if [[ -e $fake_env/require-native-ready ]]; then
                    test -f "$fake_env/native.ready"
                fi
                [[ ! -e $fake_env/fail-uv-sync ]] || exit 1
                cp "$lock_file" "$fake_env/uv-sync.txt"
                ;;
            *) exit 2 ;;
        esac
        ;;
    *) exit 2 ;;
esac
)";
        output.close();
        make_executable(path);
    }

    std::string python_environment_sync_command(const std::filesystem::path& target,
                                                const std::filesystem::path& runtime,
                                                const std::filesystem::path& uv) {
        return "KEZ_HOME=" + shell_single_quote(KEZ_SOURCE_DIR) +
               " KEZ_UV=" + shell_single_quote(uv.string()) + " bash " +
               shell_single_quote(std::string(KEZ_SOURCE_DIR) + "/scripts/python_env.sh") +
               " sync-plan " + shell_single_quote(target.string()) + " " +
               shell_single_quote(runtime.string());
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

    TEST(CommandLineParser, WritesPythonEnvironmentMetadata) {
        const std::filesystem::path directory =
            std::filesystem::temp_directory_path() /
            ("kez-python-plan-test-" + std::to_string(getpid()));
        const std::filesystem::path path = directory / "plan.sh";
        std::filesystem::remove_all(directory);

        PackageCommands package {"demo", {"build"}, {"python"}};
        package.python_distribution         = "demo==1.2.3";
        package.requires_python_environment = true;
        write_install_plan({package}, path);

        const std::string plan = read_file(path.string());
        EXPECT_NE(plan.find("kez_plan_python_distribution 'demo==1.2.3'\n"), std::string::npos);
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

    TEST(CommandLineParser, PythonEnvironmentHelperMergesAndRemovesDistributions) {
        const std::filesystem::path directory =
            std::filesystem::temp_directory_path() /
            ("kez-python-environment-test-" + std::to_string(getpid()));
        const std::filesystem::path target      = directory / "target";
        const std::filesystem::path runtime     = target / ".tmp/plan";
        const std::filesystem::path base_python = target / "python/bin/python3";
        const std::filesystem::path uv          = directory / "uv";
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(base_python.parent_path());
        std::filesystem::create_directories(runtime / "0");
        std::filesystem::create_directories(runtime / "1");
        std::filesystem::create_directories(runtime / "2");

        write_fake_python(base_python);
        write_fake_uv(uv);

        {
            write_file(runtime / "0/package-name", "python\n");
            write_file(runtime / "0/python-requirements", "");
            write_file(runtime / "0/provides-python-environment", "");
            write_file(runtime / "1/package-name", "demo\n");
            write_file(runtime / "1/uses-python-environment", "");
            write_file(runtime / "1/python-requirements", "demo==1.2.3\n");
            write_file(runtime / "2/package-name", "other\n");
            write_file(runtime / "2/uses-python-environment", "");
            write_file(runtime / "2/python-requirements", "other==4.5.6\n");
        }

        const std::string helper = python_environment_sync_command(target, runtime, uv);
        ASSERT_EQ(std::system(helper.c_str()), 0);
        EXPECT_TRUE(std::filesystem::is_regular_file(target / ".venv/bin/python"));
        EXPECT_EQ(read_file((target / ".kez-python/requirements.txt").string()),
                  "demo==1.2.3\nother==4.5.6\n");
        EXPECT_EQ(read_file((target / ".kez-python/resolved.txt").string()),
                  "demo==1.2.3\nother==4.5.6\n");
        EXPECT_EQ(read_file((target / "uv-sync.txt").string()), "demo==1.2.3\nother==4.5.6\n");
        EXPECT_TRUE(
            std::filesystem::is_regular_file(target / ".kez-python/packages/demo.requirements"));

        const std::string remove =
            "KEZ_HOME=" + shell_single_quote(KEZ_SOURCE_DIR) +
            " KEZ_UV=" + shell_single_quote(uv.string()) + " bash " +
            shell_single_quote(std::string(KEZ_SOURCE_DIR) + "/scripts/python_env.sh") +
            " remove-package " + shell_single_quote(target.string()) + " demo";
        ASSERT_EQ(std::system(remove.c_str()), 0);
        EXPECT_TRUE(std::filesystem::is_directory(target / ".venv"));
        EXPECT_EQ(read_file((target / ".kez-python/requirements.txt").string()), "other==4.5.6\n");

        std::filesystem::remove_all(directory);
    }

    TEST(CommandLineParser, PythonEnvironmentHelperCreatesEmptyUvEnvironment) {
        const std::filesystem::path directory =
            std::filesystem::temp_directory_path() /
            ("kez-empty-python-environment-test-" + std::to_string(getpid()));
        const std::filesystem::path target      = directory / "target";
        const std::filesystem::path runtime     = target / ".tmp/plan";
        const std::filesystem::path base_python = target / "python/bin/python3";
        const std::filesystem::path uv          = directory / "uv";
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(base_python.parent_path());
        std::filesystem::create_directories(runtime / "0");
        write_fake_python(base_python);
        write_fake_uv(uv);

        write_file(runtime / "0/package-name", "python\n");
        write_file(runtime / "0/python-requirements", "");
        write_file(runtime / "0/provides-python-environment", "");

        const std::string helper = python_environment_sync_command(target, runtime, uv);
        ASSERT_EQ(std::system(helper.c_str()), 0);
        EXPECT_TRUE(std::filesystem::is_regular_file(target / ".venv/bin/python"));
        EXPECT_EQ(read_file((target / ".kez-python/requirements.txt").string()), "");
        EXPECT_EQ(read_file((target / ".kez-python/resolved.txt").string()), "");
        EXPECT_FALSE(std::filesystem::exists(target / "uv-compile-ran"));
        EXPECT_FALSE(std::filesystem::exists(target / "uv-sync.txt"));

        std::filesystem::remove_all(directory);
    }

    TEST(CommandLineParser, PythonEnvironmentHelperRestoresVenvAfterUvSyncFailure) {
        const std::filesystem::path directory =
            std::filesystem::temp_directory_path() /
            ("kez-python-rollback-test-" + std::to_string(getpid()));
        const std::filesystem::path target      = directory / "target";
        const std::filesystem::path runtime     = target / ".tmp/plan";
        const std::filesystem::path base_python = target / "python/bin/python3";
        const std::filesystem::path uv          = directory / "uv";
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(base_python.parent_path());
        std::filesystem::create_directories(runtime / "0");
        std::filesystem::create_directories(runtime / "1");
        write_fake_python(base_python);
        write_fake_uv(uv);

        write_file(runtime / "0/package-name", "python\n");
        write_file(runtime / "0/python-requirements", "");
        write_file(runtime / "0/provides-python-environment", "");
        write_file(runtime / "1/package-name", "demo\n");
        write_file(runtime / "1/uses-python-environment", "");
        write_file(runtime / "1/python-requirements", "demo==1.2.3\n");

        const std::string helper = python_environment_sync_command(target, runtime, uv);
        ASSERT_EQ(std::system(helper.c_str()), 0);
        // std::ofstream(target / ".venv/original-marker");
        // std::ofstream(runtime / "1/python-requirements") << "demo==2.0.0\n";
        // std::ofstream(target / "fail-uv-sync");
        write_file(target / ".venv/original-marker", "");
        write_file(runtime / "1/python-requirements", "demo==2.0.0\n");
        write_file(target / "fail-uv-sync", "");

        EXPECT_NE(std::system(helper.c_str()), 0);
        EXPECT_TRUE(std::filesystem::is_regular_file(target / ".venv/original-marker"));
        EXPECT_EQ(read_file((target / ".kez-python/requirements.txt").string()), "demo==1.2.3\n");
        EXPECT_EQ(read_file((target / ".kez-python/resolved.txt").string()), "demo==1.2.3\n");

        std::filesystem::remove_all(directory);
    }

    TEST(CommandLineParser, BashExecutorSchedulesPythonEnvironmentBeforeConsumers) {
        const std::filesystem::path directory =
            std::filesystem::temp_directory_path() /
            ("kez-python-executor-test-" + std::to_string(getpid()));
        const std::filesystem::path target      = directory / "target";
        const std::filesystem::path plan        = directory / "plan.sh";
        const std::filesystem::path observation = directory / "virtual-environment";
        const std::filesystem::path base_python = target / "python/bin/python3";
        const std::filesystem::path uv          = directory / "uv";
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(base_python.parent_path());
        write_fake_python(base_python);
        write_fake_uv(uv);

        write_file(target / "require-native-ready", "");
        PackageCommands native {
            "native",
            {"sleep 0.1; touch " + shell_single_quote((target / "native.ready").string())},
            {}};
        PackageCommands python {"python", {"true"}, {}};
        PackageCommands demo {"demo",
                              {"mkdir -p " + shell_single_quote((target / "demo").string())},
                              {"python", "native"},
                              "demo==1.2.3",
                              true};
        PackageCommands application {
            "application",
            {"mkdir -p " +
             shell_single_quote((target / "application/lib/python3.12/site-packages").string()) +
             " && printf '%s\\n' \"$VIRTUAL_ENV\" > " + shell_single_quote(observation.string())},
            {"demo"}};
        application.requires_python_environment = true;
        write_install_plan({native, python, demo, application}, plan);

        const std::string executor =
            "KEZ_HOME=" + shell_single_quote(KEZ_SOURCE_DIR) +
            " KEZ_UV=" + shell_single_quote(uv.string()) + " KEZ_NJOBS=2 env -u KEZ_WORKDIR bash " +
            shell_single_quote(std::string(KEZ_SOURCE_DIR) + "/scripts/install.sh") + " " +
            shell_single_quote(target.string()) + " " + shell_single_quote(plan.string());
        ASSERT_EQ(std::system(executor.c_str()), 0);
        EXPECT_EQ(read_file(observation.string()), (target / ".venv").string() + "\n");
        EXPECT_EQ(read_file((target / "state.yaml").string()),
                  "state:\n  - native\n  - python\n  - demo\n  - application\n");
        EXPECT_TRUE(std::filesystem::is_regular_file(target / ".venv/bin/python"));
        EXPECT_EQ(read_file((target / "uv-sync.txt").string()), "demo==1.2.3\n");
        EXPECT_EQ(read_file((target / ".venv/lib/python3.12/site-packages/"
                                      "kez-application.pth")
                                .string()),
                  (target / "application/lib/python3.12/site-packages").string() + "\n");

        std::filesystem::remove_all(directory);
    }

    TEST(CommandLineParser, ModulefileActivatesPythonVirtualEnvironmentFirst) {
        const std::filesystem::path directory =
            std::filesystem::temp_directory_path() /
            ("kez-python-modulefile-test-" + std::to_string(getpid()));
        const std::filesystem::path environment = directory / "example";
        const std::filesystem::path modulefile  = directory / "modulefile";
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(environment / "application/bin");
        std::filesystem::create_directories(environment / ".venv/bin");

        const std::string generate =
            "bash " +
            shell_single_quote(std::string(KEZ_SOURCE_DIR) + "/scripts/gen_modulefile.sh") + " " +
            shell_single_quote(environment.string()) + " > " +
            shell_single_quote(modulefile.string());
        ASSERT_EQ(std::system(generate.c_str()), 0);

        const std::string contents = read_file(modulefile.string());
        const std::string package_path =
            "prepend-path PATH \"" + (environment / "application/bin").string() + "\"";
        const std::string virtual_path =
            "prepend-path PATH \"" + (environment / ".venv/bin").string() + "\"";
        EXPECT_NE(contents.find("setenv VIRTUAL_ENV \"" + (environment / ".venv").string() + "\""),
                  std::string::npos);
        ASSERT_NE(contents.find(package_path), std::string::npos);
        ASSERT_NE(contents.find(virtual_path), std::string::npos);
        EXPECT_LT(contents.find(package_path), contents.find(virtual_path));

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

        for (const char* package : {"elfutils", "m4", "autoconf", "automake", "libtool", "make",
                                    "perl", "git", "yaml-cpp", "googletest"}) {
            const std::string block = install_plan_package_block(contents, package);
            ASSERT_FALSE(block.empty()) << package;
            EXPECT_NE(block.find("kez_plan_depends gcc\n"), std::string::npos) << package;
        }

        for (const char* package : {"cmake", "rust", "patchelf"}) {
            const std::string block = install_plan_package_block(contents, package);
            ASSERT_FALSE(block.empty()) << package;
            EXPECT_EQ(block.find("kez_plan_depends gcc\n"), std::string::npos) << package;
        }

        const std::string uv = install_plan_package_block(contents, "uv");
        ASSERT_FALSE(uv.empty());
        EXPECT_NE(uv.find("--build-package\\ uv"), std::string::npos);
        EXPECT_NE(uv.find("kez_plan_depends python\n"), std::string::npos);
        EXPECT_EQ(uv.find("kez_plan_depends gcc\n"), std::string::npos);

        const std::string meson = install_plan_package_block(contents, "meson");
        EXPECT_NE(meson.find("kez_plan_depends python\n"), std::string::npos);
        EXPECT_NE(meson.find("kez_plan_depends uv\n"), std::string::npos);
        EXPECT_NE(meson.find("kez_plan_depends ninja\n"), std::string::npos);

        std::filesystem::remove_all(directory);
    }

    TEST(CommandLineParser, BashExecutorRunsReadyPackagesAndWritesLogs) {
        const std::filesystem::path directory =
            std::filesystem::temp_directory_path() /
            ("kez-install-parallel-test-" + std::to_string(getpid()));
        const std::filesystem::path target = directory / "target env";
        const std::filesystem::path plan   = directory / "plan.sh";
        const std::filesystem::path output = directory / "executor output";
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
            "KEZ_NJOBS=2 env -u KEZ_HOME -u KEZ_WORKDIR bash " +
            shell_single_quote(std::string(KEZ_SOURCE_DIR) + "/scripts/install.sh") + " " +
            shell_single_quote(target.string()) + " " + shell_single_quote(plan.string()) + " > " +
            shell_single_quote(output.string()) + " 2>&1";

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
        const std::string console = read_file(output.string());
        EXPECT_NE(console.find("Logs are available at " + (target / "logs").string()),
                  std::string::npos);
        EXPECT_NE(console.find("[I]: base:   command 0/2 [...............]"), std::string::npos);
        EXPECT_NE(console.find("[I]: right:  command 0/3 [...............]"), std::string::npos);
        EXPECT_NE(console.find("[S]: base:   command 2/2 [###############]"), std::string::npos);
        EXPECT_NE(console.find("[S]: top:    command 3/3 [###############]"), std::string::npos);

        std::filesystem::remove_all(directory);
    }

    TEST(CommandLineParser, BashExecutorAnimatesInteractivePackageProgress) {
        if (std::system("command -v script >/dev/null 2>&1") != 0) {
            GTEST_SKIP() << "util-linux script is unavailable";
        }

        const std::filesystem::path directory =
            std::filesystem::temp_directory_path() /
            ("kez-install-progress-test-" + std::to_string(getpid()));
        const std::filesystem::path target     = directory / "target";
        const std::filesystem::path plan       = directory / "plan.sh";
        const std::filesystem::path transcript = directory / "typescript";
        std::filesystem::remove_all(directory);

        write_install_plan(
            {{"left", {"sleep 0.3", "true"}, {}}, {"right", {"sleep 0.3", "true"}, {}}}, plan);
        const std::string install_command =
            "bash " + shell_single_quote(std::string(KEZ_SOURCE_DIR) + "/scripts/install.sh") +
            " " + shell_single_quote(target.string()) + " " + shell_single_quote(plan.string());
        const std::string executor = "TERM=xterm KEZ_NJOBS=2 env -u KEZ_HOME -u KEZ_WORKDIR "
                                     "script --quiet --return --command " +
                                     shell_single_quote(install_command) + " " +
                                     shell_single_quote(transcript.string()) + " >/dev/null 2>&1";

        ASSERT_EQ(std::system(executor.c_str()), 0);
        const std::string console = read_file(transcript.string());
        EXPECT_NE(console.find("\033[34m[I]: left:   command 0/2 [-..............]\033[0m"),
                  std::string::npos);
        EXPECT_NE(console.find("\033[34m[I]: right:  command 0/2 [\\..............]\033[0m"),
                  std::string::npos);
        EXPECT_NE(console.find("\033[32m[S]: left:   command 2/2 [###############]\033[0m"),
                  std::string::npos);
        EXPECT_NE(console.find("\033[32m[S]: right:  command 2/2 [###############]\033[0m"),
                  std::string::npos);

        std::filesystem::remove_all(directory);
    }

    TEST(CommandLineParser, BashExecutorAlignsProgressWithDifferentStepWidths) {
        const std::filesystem::path directory =
            std::filesystem::temp_directory_path() /
            ("kez-install-alignment-test-" + std::to_string(getpid()));
        const std::filesystem::path target = directory / "target";
        const std::filesystem::path plan   = directory / "plan.sh";
        const std::filesystem::path output = directory / "executor output";
        std::filesystem::remove_all(directory);

        write_install_plan(
            {{"short",
              {"true", "true", "true", "true", "true", "true", "true", "true", "true"},
              {}},
             {"long",
              {"true", "true", "true", "true", "true", "true", "true", "true", "true", "true"},
              {}}},
            plan);
        const std::string executor =
            "KEZ_NJOBS=2 env -u KEZ_HOME -u KEZ_WORKDIR bash " +
            shell_single_quote(std::string(KEZ_SOURCE_DIR) + "/scripts/install.sh") + " " +
            shell_single_quote(target.string()) + " " + shell_single_quote(plan.string()) + " > " +
            shell_single_quote(output.string()) + " 2>&1";

        ASSERT_EQ(std::system(executor.c_str()), 0);
        const std::string console = read_file(output.string());
        EXPECT_NE(console.find("[I]: short:  command  0/ 9 [...............]"), std::string::npos);
        EXPECT_NE(console.find("[I]: long:   command  0/10 [...............]"), std::string::npos);
        EXPECT_NE(console.find("[S]: short:  command  9/ 9 [###############]"), std::string::npos);
        EXPECT_NE(console.find("[S]: long:   command 10/10 [###############]"), std::string::npos);

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
            "KEZ_NJOBS=2 env -u KEZ_HOME -u KEZ_WORKDIR bash " +
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

    TEST(ShellColoredIo, FormatsRedirectedMessagesWithoutAnsiCodes) {
        const std::filesystem::path directory = std::filesystem::temp_directory_path() /
                                                ("kez-colored-io-test-" + std::to_string(getpid()));
        const std::filesystem::path output    = directory / "output";
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(directory);

        const std::string commands =
            "source " + shell_single_quote(std::string(KEZ_SOURCE_DIR) + "/scripts/colored_io.sh") +
            "; info 'information'; warning 'warning'; error 'error'; success 'success'";
        const std::string executor = "TERM=xterm bash -c " + shell_single_quote(commands) + " > " +
                                     shell_single_quote(output.string()) + " 2>&1";

        ASSERT_EQ(std::system(executor.c_str()), 0);
        EXPECT_EQ(read_file(output.string()),
                  "[I]: information\n[W]: warning\n[E]: error\n[S]: success\n");

        std::filesystem::remove_all(directory);
    }

    TEST(ShellColoredIo, InitUsesShellErrorsAndKeepsBinaryCompletionCheck) {
        const std::filesystem::path directory =
            std::filesystem::temp_directory_path() /
            ("kez-init-colored-io-test-" + std::to_string(getpid()));
        const std::filesystem::path output = directory / "output";
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(directory);

        const std::filesystem::path init_path =
            std::filesystem::path(KEZ_SOURCE_DIR) / "scripts" / "init.sh";
        const std::string executor = "TERM=xterm bash " + shell_single_quote(init_path.string()) +
                                     " --invalid > " + shell_single_quote(output.string()) +
                                     " 2>&1";
        EXPECT_NE(std::system(executor.c_str()), 0);
        EXPECT_EQ(read_file(output.string()), "[E]: Unknown init option: --invalid\n");

        const std::string init_script = read_file(init_path.string());
        EXPECT_NE(init_script.find("\"${KEZ_HOME}/bin/kez_print\" success \"Kez environment "
                                   "initialization complete.\""),
                  std::string::npos);

        std::filesystem::remove_all(directory);
    }

}  // namespace
