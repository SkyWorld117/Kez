#include <gtest/gtest.h>
#include <unistd.h>

#include <cstdlib>
#include <database/database.hpp>
#include <filesystem>
#include <fstream>
#include <optional>
#include <parser/user_config_parser.hpp>
#include <string>
#include <user_config_generator/user_config_generator.hpp>
#include <vector>

namespace {

    class TemporaryUserConfigParserDatabase : public ::testing::Test {
       protected:
        void SetUp() override {
            const char* database = std::getenv("KEZ_DB");
            const char* home     = std::getenv("KEZ_HOME");
            const char* arch     = std::getenv("KEZ_ARCH");
            const char* flags    = std::getenv("CFLAGS");
            if (database != nullptr) {
                previous_database_ = database;
            }
            if (home != nullptr) {
                previous_home_ = home;
            }
            if (arch != nullptr) {
                previous_architecture_ = arch;
            }
            if (flags != nullptr) {
                previous_cflags_ = flags;
            }

            path_ = std::filesystem::temp_directory_path() /
                    ("kez-user-config-parser-test-" + std::to_string(getpid()));
            std::filesystem::remove_all(path_);
            std::filesystem::create_directories(path_ / "database");
            std::filesystem::create_directories(path_ / "patches" / "application");
            setenv("KEZ_DB", (path_ / "database").c_str(), 1);
            setenv("KEZ_HOME", path_.c_str(), 1);
            setenv("KEZ_ARCH", "x86_64", 1);
            setenv("CFLAGS", "host flags", 1);
            clear_db_cache();
        }

        void TearDown() override {
            clear_db_cache();
            std::filesystem::remove_all(path_);
            restore("KEZ_DB", previous_database_);
            restore("KEZ_HOME", previous_home_);
            restore("KEZ_ARCH", previous_architecture_);
            restore("CFLAGS", previous_cflags_);
        }

        static void restore(const char* name, const std::optional<std::string>& value) {
            if (value.has_value()) {
                setenv(name, value->c_str(), 1);
            } else {
                unsetenv(name);
            }
        }

        void write_package(const std::string& package, const std::string& contents) const {
            write_package(package, "latest.yaml", contents);
        }

        void write_package(const std::string& package, const std::string& filename,
                           const std::string& contents) const {
            const std::filesystem::path directory = path_ / "database" / package;
            std::filesystem::create_directories(directory);
            std::ofstream output(directory / filename);
            ASSERT_TRUE(output.good());
            output << contents;
        }

        void write_file(const std::filesystem::path& path, const std::string& contents) const {
            std::ofstream output(path);
            ASSERT_TRUE(output.good());
            output << contents;
        }

        UserConfigParserSettings settings() const {
            UserConfigParserSettings result;
            result.install_prefix   = "/opt/env";
            result.kez_home         = path_;
            result.system_prefix    = "/opt/system";
            result.compilers_prefix = "/opt/compilers";
            result.mpis_prefix      = "/opt/mpis";
            result.vendors_prefix   = "/opt/vendors";
            result.parallel_jobs    = 8;
            result.architecture     = "x86_64";
            return result;
        }

        std::filesystem::path path_;
        std::optional<std::string> previous_database_;
        std::optional<std::string> previous_home_;
        std::optional<std::string> previous_architecture_;
        std::optional<std::string> previous_cflags_;
    };

    TEST_F(TemporaryUserConfigParserDatabase,
           GeneratesDependencyOrderedCommandsFromTypedRecipesAndUserValues) {
        write_file(path_ / "patches" / "application" / "fix.patch", "patch");
        write_package("application", R"(
recipe:
  name: application
  type: package
  toolchain: cmake
  source:
    type: tarball
    releases:
      - version: 2.0
        url: https://example.invalid/application-${kez.arch}.tar.gz
  dependencies: [library]
  overrides:
    - condition: ${application.config.feature} true
      target: ${library.libs}
      action: append
      value: -loverride
  build:
    preprocessing: prepare ${source}
    postprocessing: finish ${application.prefix}
    configurations:
      environment:
        - name: CFLAGS
          user_configurable: true
          default: -O3
      options:
        - name: feature
          user_configurable: true
          enabled:
            default: false
          enabled_format: -DFEATURE=ON
          disabled_format: -DFEATURE=OFF
        - name: -DLINK_FLAGS
          enabled_value:
            default: ${library.libs}
    stages:
      - target: build
      - target: install
)");
        write_package("library", R"(
recipe:
  name: library
  type: package
  toolchain: make
  source:

    type: git
    url: https://example.invalid/library.git
    releases:
      - version: v1
        tag: v1
  build:
    stages:
      - target:
      - target: install
  properties:
    libs:
      default: -lbase
      conditions:
        - condition: ${application.config.feature} true
          action: append
          value: -lfeature
)");

        YAML::Node user_config               = gen_user_config({"application"}, false, "system");
        YAML::Node application               = user_config["cheese"]["application"];
        application["patches"][0]["enabled"] = true;
        application["build"]["configurations"]["environment"][0]["value"] = "-O2 ${PATH}";
        application["build"]["configurations"]["options"][0]["enabled"]   = true;

        const BashCommandPlan plan = parse_user_config(user_config, settings());

        ASSERT_EQ(plan.size(), 2U);
        EXPECT_EQ(plan[0].package, "library");
        EXPECT_EQ(
            plan[0].commands,
            std::vector<std::string>({"bash '" + (path_ / "tools" / "shallow_clone.sh").string() +
                                          "' 'https://example.invalid/library.git' 'v1' source",
                                      "cd source", "make -j8", "make -j8 install"}));

        EXPECT_EQ(plan[1].package, "application");
        const std::vector<std::string>& commands = plan[1].commands;
        ASSERT_EQ(commands.size(), 12U);
        EXPECT_EQ(commands[0], "wget --quiet --show-progress --no-check-certificate "
                               "--output-document='source.tar.gz' "
                               "'https://example.invalid/application-x86_64.tar.gz'");
        EXPECT_EQ(commands[1],
                  "bash '" + (path_ / "tools" / "unpack.sh").string() + "' 'source.tar.gz' source");
        EXPECT_EQ(commands[2], "rm 'source.tar.gz'");
        EXPECT_EQ(commands[3], "cd source");
        EXPECT_EQ(commands[4],
                  "git apply '" + (path_ / "patches" / "application" / "fix.patch").string() + "'");
        EXPECT_EQ(commands[5], "prepare /opt/env/.tmp/source");
        EXPECT_EQ(commands[6], "export CFLAGS=\"-O2 ${PATH}\"");
        EXPECT_EQ(commands[7], "cmake -B build -DFEATURE=ON "
                               "-DLINK_FLAGS=\"-lbase -lfeature -loverride\" "
                               "-DCMAKE_PREFIX_PATH='/opt/env' -DCMAKE_BUILD_TYPE=Release");
        EXPECT_EQ(commands[8], "export CFLAGS='host flags'");
        EXPECT_EQ(commands[9], "cmake --build build --parallel 8 --target build");
        EXPECT_EQ(commands[10], "cmake --install build");
        EXPECT_EQ(commands[11], "finish /opt/env");

        const std::filesystem::path local_source = path_ / "local source";
        std::filesystem::create_directories(local_source);
        user_config["cheese"]["application"]["version"] = "dev@" + local_source.string();
        const BashCommandPlan local_plan = parse_user_config(user_config, settings());
        ASSERT_EQ(local_plan.size(), 2U);
        ASSERT_FALSE(local_plan[1].commands.empty());
        EXPECT_EQ(local_plan[1].commands[0], "cp -a '" + local_source.string() + "' source");
    }

    TEST_F(TemporaryUserConfigParserDatabase, ResolvesAbstractPropertiesAndSelectionConditions) {
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
  properties:
    c: /opt/mpi/bin/mpicc
)");
        write_package("application", R"(
recipe:
  name: application
  type: package
  build:
    configurations:
      command: configure
      options:
        - name: CC
          enabled_value:
            default: ${mpi.c}
        - name: --with-mpi
          enabled:
            conditions:
              - condition: ${mpi.use-implementation} true
                value: true
)");

        const YAML::Node user_config = YAML::Load(R"(
cheese:
  application:
    compiler: system
  implementation: {}
recipe:
  abstract_packages:
    mpi: implementation
  dependencies: [application, implementation]
  targets: [application]
)");

        const BashCommandPlan plan = parse_user_config(user_config, settings());

        ASSERT_EQ(plan.size(), 1U);
        EXPECT_EQ(plan[0].package, "application");
        ASSERT_EQ(plan[0].commands.size(), 1U);
        EXPECT_EQ(plan[0].commands[0], "configure CC=\"/opt/mpi/bin/mpicc\" --with-mpi");
    }

    TEST_F(TemporaryUserConfigParserDatabase, SelectsVersionedRecipeAndResolvesItsCanonicalName) {
        write_package("demo", R"(
recipe:
  name: demo
  type: package
)");
        write_package("demo", "1.0-1.9.yaml", R"(
recipe:
  name: demo-v1
  type: package
  build:
    configurations:
      command: echo ${demo-v1.prefix}
)");
        const YAML::Node user_config = YAML::Load(R"(
cheese:
  demo:
    version: 1.5
    compiler: system
recipe:
  abstract_packages: {}
  dependencies: [demo]
  targets: [demo]
)");

        const BashCommandPlan plan = parse_user_config(user_config, settings());

        ASSERT_EQ(plan.size(), 1U);
        EXPECT_EQ(plan[0].package, "demo");
        EXPECT_EQ(plan[0].commands, std::vector<std::string>({"echo /opt/env"}));
    }

    TEST_F(TemporaryUserConfigParserDatabase, ParsesARepositoryRecipeEndToEnd) {
        const std::filesystem::path source = KEZ_SOURCE_DIR;
        setenv("KEZ_DB", (source / "database").c_str(), 1);
        setenv("KEZ_HOME", source.c_str(), 1);
        clear_db_cache();

        const YAML::Node user_config             = gen_user_config({"fio"}, false, "system");
        UserConfigParserSettings parser_settings = settings();
        parser_settings.kez_home                 = source;
        const BashCommandPlan plan               = parse_user_config(user_config, parser_settings);

        ASSERT_EQ(plan.size(), 2U);
        EXPECT_EQ(plan[0].package, "libaio");
        EXPECT_EQ(plan[1].package, "fio");
        for (const PackageCommands& package : plan) {
            for (const std::string& command : package.commands) {
                EXPECT_EQ(command.find("${fio."), std::string::npos);
                EXPECT_EQ(command.find("${libaio."), std::string::npos);
            }
        }

        testing::internal::CaptureStdout();
        const YAML::Node complex_user_config = gen_user_config({"dftbplus"}, false, "system");
        testing::internal::GetCapturedStdout();
        parser_settings.external_packages["rdma-core"] = {"/usr", "1"};
        const BashCommandPlan complex_plan =
            parse_user_config(complex_user_config, parser_settings);
        EXPECT_GE(complex_plan.size(), 3U);
        for (const PackageCommands& package : complex_plan) {
            for (const std::string& command : package.commands) {
                std::size_t position = 0;
                while ((position = command.find("${", position)) != std::string::npos) {
                    const std::size_t closing = command.find('}', position + 2);
                    ASSERT_NE(closing, std::string::npos);
                    EXPECT_EQ(command.substr(position + 2, closing - position - 2).find('.'),
                              std::string::npos)
                        << command;
                    position = closing + 1;
                }
            }
        }
    }

}  // namespace
