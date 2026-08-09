/**
 * @file user_config_parser_test.cpp
 * @brief Tests for the user configuration parser that translates user-editable
 *        YAML configs into executable BashCommandPlans.
 *
 * Each test creates a temporary database with minimal synthetic recipes and
 * verifies that parse_user_config() produces the correct commands, dependency
 * ordering, template resolution, condition evaluation, and error handling.
 */

#include <gtest/gtest.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <database/database.hpp>
#include <filesystem>
#include <fstream>
#include <optional>
#include <rebuild/rebuild.hpp>
#include <string>
#include <uconf_generator/uconf_generator.hpp>
#include <uconf_parser/user_config_parser.hpp>
#include <utils/bash_utils.hpp>
#include <utils/file_utils.hpp>
#include <vector>

namespace {

    /**
     * @brief Test fixture that provisions a temporary, isolated database
     *        environment for user-config-parser tests.
     *
     * SetUp() creates a temporary directory with a minimal database (one
     * synthetic `gcc` compiler recipe) and sets the KEZ_DB, KEZ_HOME,
     * KEZ_ARCH, and CFLAGS environment variables to point into it.  It
     * saves any pre-existing values for those variables and restores them
     * in TearDown().
     *
     * The fixture provides convenience methods:
     *   - write_package()    – write a recipe YAML into the temp database.
     *   - write_file()       – write an arbitrary file under the temp root.
     *   - settings()         – return a fully populated UserConfigParserSettings
     *                          struct pointing into the temp directory.
     *
     * Every test starts with a clean temp directory and a known compiler
     * ("gcc") so that `gen_user_config()` can resolve the compiler property
     * used during command generation.
     */
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
            setenv("KEZ_DB", (path_ / "database").c_str(), 1);
            setenv("KEZ_HOME", path_.c_str(), 1);
            setenv("KEZ_ARCH", "x86_64", 1);
            setenv("CFLAGS", "host flags", 1);

            const std::string gcc_recipe = R"(
recipe:
  name: gcc
  type: compiler
  properties:
    c: /usr/bin/gcc
    cxx: /usr/bin/g++
    fort: /usr/bin/gfortran
)";

            // Provide a manifest.yaml so that parse_compiler("system") can
            // look up the system gcc version.  The version "11.5.0" will
            // not match any version-range file, so select_config_path falls
            // back to latest.yaml — no extra recipe file is needed.
            write_file(path_ / "manifest.yaml", "system-stack:\n  gcc: 11.5.0\n");
            write_package("gcc", gcc_recipe);
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
            std::filesystem::create_directories(path.parent_path());
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
            result.cache_prefix     = "/opt/.cache";
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

    YAML::Node find_option(YAML::Node options, const std::string& name) {
        for (YAML::Node option : options) {
            if (option["name"].as<std::string>() == name) {
                return option;
            }
        }
        ADD_FAILURE() << "Missing option: " << name;
        return YAML::Node();
    }

    TEST_F(TemporaryUserConfigParserDatabase,
           GeneratesDependencyOrderedCommandsFromTypedRecipesAndUserValues) {
        write_file(path_ / "database" / "application" / "patches" / "fix.patch", "patch");
        write_file(path_ / "database" / "application" / "patches" / "_rules.yaml", R"(
patches:
  - name: fix.patch
    enabled: false
    versions: ["==2.0"]
)");
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
          enabled_format: FEATURE=ON
          disabled_format: FEATURE=OFF
        - name: LINK_FLAGS
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
        YAML::Node application               = user_config["kez"]["application"];
        application["patches"][0]["enabled"] = true;
        application["build"]["configurations"]["environment"][0]["value"] = "-O2 ${PATH}";
        application["build"]["configurations"]["options"][0]["enabled"]   = true;

        const BashCommandPlan plan = parse_user_config(user_config, settings());

        ASSERT_EQ(plan.size(), 2U);
        EXPECT_EQ(plan[0].package, "library");
        EXPECT_TRUE(plan[0].dependencies.empty());
        EXPECT_EQ(plan[0].commands,
                  std::vector<std::string>(
                      {"bash '" + (path_ / "tools" / "shallow_clone.sh").string() +
                           "' 'https://example.invalid/library.git' 'v1' source",
                       "mkdir -p '/opt/.cache'",
                       "tar -czf '/opt/.cache/library-v1.tar.gz' --format=posix -z source",
                       "cd source", "make -j8", "make -j8 install"}));

        EXPECT_EQ(plan[1].package, "application");
        EXPECT_EQ(plan[1].dependencies, std::vector<std::string>({"library"}));
        const std::vector<std::string>& commands = plan[1].commands;
        ASSERT_EQ(commands.size(), 14U);
        EXPECT_EQ(commands[0], "wget --quiet --show-progress --no-check-certificate "
                               "--output-document='source.tar.gz' "
                               "'https://example.invalid/application-x86_64.tar.gz'");
        EXPECT_EQ(commands[1],
                  "bash '" + (path_ / "tools" / "unpack.sh").string() + "' 'source.tar.gz' source");
        EXPECT_EQ(commands[2], "rm 'source.tar.gz'");
        EXPECT_EQ(commands[3], "mkdir -p '/opt/.cache'");
        EXPECT_EQ(commands[4],
                  "tar -czf '/opt/.cache/application-2.0.tar.gz' --format=posix -z source");
        EXPECT_EQ(commands[5], "cd source");
        EXPECT_EQ(commands[6],
                  "if [[ -e .git ]]; then git apply '" +
                      (path_ / "database" / "application" / "patches" / "fix.patch").string() +
                      "'; else patch --batch --forward -p1 --input='" +
                      (path_ / "database" / "application" / "patches" / "fix.patch").string() +
                      "'; fi");
        EXPECT_EQ(commands[7], "prepare /opt/env/.tmp/application/source");
        EXPECT_EQ(commands[8], "export CFLAGS=\"-O2 ${PATH}\"");
        EXPECT_EQ(commands[9], "cmake -B build -DFEATURE=ON "
                               "-DLINK_FLAGS=\"-lbase -lfeature -loverride\" "
                               "-DCMAKE_INSTALL_PREFIX=\"/opt/env/application\" "
                               "-DCMAKE_BUILD_TYPE=\"Release\" -DCMAKE_C_COMPILER=\"/usr/bin/gcc\" "
                               "-DCMAKE_CXX_COMPILER=\"/usr/bin/g++\" "
                               "-DCMAKE_Fortran_COMPILER=\"/usr/bin/gfortran\" "
                               "-DCMAKE_C_FLAGS=\"-O3 -march=native -mtune=native\" "
                               "-DCMAKE_CXX_FLAGS=\"-O3 -march=native -mtune=native\" "
                               "-DCMAKE_Fortran_FLAGS=\"-O3 -march=native -mtune=native\" "
                               "-DCMAKE_CUDA_FLAGS=\"-O3 -Xptxas -O3\" "
                               "-DCMAKE_EXE_LINKER_FLAGS=\"-lbase -lfeature -loverride\" "
                               "-DCMAKE_SHARED_LINKER_FLAGS=\"-lbase -lfeature -loverride\" "
                               "-DCMAKE_MODULE_LINKER_FLAGS=\"-lbase -lfeature -loverride\" "
                               "-DCMAKE_PREFIX_PATH=\"/opt/env/library\"");
        EXPECT_EQ(commands[10], "export CFLAGS='host flags'");
        EXPECT_EQ(commands[11], "cmake --build build --parallel 8 --target build");
        EXPECT_EQ(commands[12], "cmake --install build");
        EXPECT_EQ(commands[13], "finish /opt/env/application");

        const std::filesystem::path local_source = path_ / "local source";
        std::filesystem::create_directories(local_source);
        user_config["kez"]["application"]["version"] = "dev@" + local_source.string();
        const BashCommandPlan local_plan             = parse_user_config(user_config, settings());
        ASSERT_EQ(local_plan.size(), 2U);
        ASSERT_FALSE(local_plan[1].commands.empty());
        EXPECT_EQ(local_plan[1].commands[0], "cp -a '" + local_source.string() + "' source");
    }

    TEST_F(TemporaryUserConfigParserDatabase, AppliesPatchToNonGitLocalSource) {
        const std::filesystem::path local_source = path_ / "local source";
        const std::filesystem::path build_dir    = path_ / "build directory";
        std::filesystem::create_directories(local_source);
        std::filesystem::create_directories(build_dir);
        write_file(local_source / "message.txt", "before\n");
        write_file(path_ / "database" / "application" / "patches" / "fix.patch",
                   R"(--- a/message.txt
+++ b/message.txt
@@ -1 +1 @@
-before
+after
)");
        write_file(path_ / "database" / "application" / "patches" / "_rules.yaml", R"(
patches:
  - name: fix.patch
    enabled: false
    versions: ["==1.0"]
)");
        write_package("application", R"(
recipe:
  name: application
  type: package
  source:
    type: tarball
    releases:
      - version: 1.0
        url: https://example.invalid/application.tar.gz
  build:
    preprocessing: "true"
)");

        YAML::Node user_config = gen_user_config({"application"}, false, "system");
        user_config["kez"]["application"]["version"]               = "dev@" + local_source.string();
        user_config["kez"]["application"]["patches"][0]["enabled"] = true;
        const BashCommandPlan plan = parse_user_config(user_config, settings());

        ASSERT_EQ(plan.size(), 1U);
        ASSERT_GE(plan[0].commands.size(), 3U);
        const std::string apply_patch = "cd " + shell_single_quote(build_dir.string()) + " && " +
                                        plan[0].commands[0] + " && " + plan[0].commands[1] +
                                        " && " + plan[0].commands[2];
        ASSERT_EQ(std::system(apply_patch.c_str()), 0);
        EXPECT_EQ(read_file((build_dir / "source" / "message.txt").string()), "after\n");
    }

    TEST_F(TemporaryUserConfigParserDatabase, ResolvesPythonEnvironmentTemplatesAndPlanMetadata) {
        write_package("python", R"(
recipe:
  name: python
  type: package
  build:
    configurations:
      command: build-python ${python.prefix}
)");
        write_package("demo", R"(
recipe:
  name: demo
  type: package
  toolchain: python
  source:
    type: pypi
    releases:
      - version: 1.2.3
)");
        write_package("application", R"(
recipe:
  name: application
  type: package
  dependencies: [demo]
  build:
    configurations:
      command: use-python ${python.executable} ${python.venv}
)");

        const YAML::Node user_config = gen_user_config({"application"}, false, "system");
        const BashCommandPlan plan   = parse_user_config(user_config, settings());

        const auto application = std::find_if(
            plan.begin(), plan.end(),
            [](const PackageCommands& package) { return package.package == "application"; });
        ASSERT_NE(application, plan.end());
        EXPECT_TRUE(application->requires_python_environment);
        EXPECT_TRUE(application->python_distribution.empty());
        EXPECT_NE(
            std::find(application->dependencies.begin(), application->dependencies.end(), "demo"),
            application->dependencies.end());
        EXPECT_NE(std::find(application->commands.begin(), application->commands.end(),
                            "use-python /opt/env/.venv/bin/python /opt/env/.venv"),
                  application->commands.end());

        const auto demo =
            std::find_if(plan.begin(), plan.end(),
                         [](const PackageCommands& package) { return package.package == "demo"; });
        ASSERT_NE(demo, plan.end());
        EXPECT_EQ(demo->python_distribution, "demo==1.2.3");
        EXPECT_TRUE(demo->requires_python_environment);
        EXPECT_NE(std::find(demo->dependencies.begin(), demo->dependencies.end(), "python"),
                  demo->dependencies.end());
        EXPECT_EQ(demo->commands, std::vector<std::string>({"mkdir -p '/opt/env/demo'"}));
    }

    TEST_F(TemporaryUserConfigParserDatabase,
           ResolvesIndexedValuesForTopLevelAndOutOfOrderStageConfigurations) {
        write_package("application", R"(
recipe:
  name: application
  type: package
  build:
    configurations:
      command: top-configure
      environment:
        - name: KEZ_INDEX_TOP_A
          user_configurable: true
          default: default-a
        - name: KEZ_INDEX_TOP_B
          user_configurable: true
          default: default-b
      options:
        - name: TOP_A
          user_configurable: true
          enabled_value:
            default: default-a
        - name: TOP_B
          user_configurable: true
          enabled_value:
            default: default-b
    stages:
      - target: build
        configurations:
          command: build-stage
          options:
            - name: BUILD_VALUE
              user_configurable: true
              enabled_value:
                default: default-build
      - target:
        configurations:
          command: default-stage
          environment:
            - name: KEZ_INDEX_STAGE_ENV
              user_configurable: true
              default: default-stage
)");

        const YAML::Node user_config = YAML::Load(R"(
kez:
  application:
    compiler: system
    build:
      configurations:
        environment:
          - name: KEZ_INDEX_TOP_B
            value: user-b
          - name: KEZ_INDEX_TOP_A
            value: user-a
        options:
          - name: TOP_B
            enabled: true
            enabled_value: user-b
            disabled_value: ~
          - name: TOP_A
            enabled: true
            enabled_value: user-a
            disabled_value: ~
      stages:
        - target:
          configurations:
            environment:
              - name: KEZ_INDEX_STAGE_ENV
                value: user-stage
        - target: build
          configurations:
            options:
              - name: BUILD_VALUE
                enabled: true
                enabled_value: user-build
                disabled_value: ~
recipe:
  abstract_packages: {}
  dependencies: [application]
  targets: [application]
)");

        unsetenv("KEZ_INDEX_TOP_A");
        unsetenv("KEZ_INDEX_TOP_B");
        unsetenv("KEZ_INDEX_STAGE_ENV");
        const BashCommandPlan plan = parse_user_config(user_config, settings());

        ASSERT_EQ(plan.size(), 1U);
        EXPECT_EQ(plan[0].commands,
                  std::vector<std::string>(
                      {"export KEZ_INDEX_TOP_A=\"user-a\"", "export KEZ_INDEX_TOP_B=\"user-b\"",
                       "top-configure TOP_A=\"user-a\" TOP_B=\"user-b\"", "unset KEZ_INDEX_TOP_A",
                       "unset KEZ_INDEX_TOP_B", "build-stage BUILD_VALUE=\"user-build\"",
                       "export KEZ_INDEX_STAGE_ENV=\"user-stage\"", "default-stage",
                       "unset KEZ_INDEX_STAGE_ENV"}));
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
kez:
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

    TEST_F(TemporaryUserConfigParserDatabase, ConvergesWhenBuildAndStageReuseAnEnvironmentName) {
        write_package("application", R"(
recipe:
  name: application
  type: package
  build:
    configurations:
      command: configure
      environment:
        - name: KEZ_DUPLICATE_SCOPE
          default: build-value
    stages:
      - target: build
        configurations:
          command: build-stage
          environment:
            - name: KEZ_DUPLICATE_SCOPE
              default: stage-value
)");

        unsetenv("KEZ_DUPLICATE_SCOPE");
        const YAML::Node user_config = gen_user_config({"application"}, false, "system");
        const BashCommandPlan plan   = parse_user_config(user_config, settings());

        ASSERT_EQ(plan.size(), 1U);
        EXPECT_EQ(plan[0].commands,
                  std::vector<std::string>({"export KEZ_DUPLICATE_SCOPE=\"build-value\"",
                                            "configure", "unset KEZ_DUPLICATE_SCOPE",
                                            "export KEZ_DUPLICATE_SCOPE=\"stage-value\"",
                                            "build-stage", "unset KEZ_DUPLICATE_SCOPE"}));
    }

    TEST_F(TemporaryUserConfigParserDatabase,
           ExpandsRawDependencyPathsAndLetsExplicitOptionsOverrideDefaults) {
        write_package("library", R"(
recipe:
  name: library
  type: package
  properties:
    include: ${library.prefix}/include
    lib: ${library.prefix}/lib64
    libs: -lexample
)");
        write_package("application", R"(
recipe:
  name: application
  type: package
  toolchain: cmake
  dependencies: [library]
  build:
    preprocessing: echo ${library.incflags} ${library.ldflags}
    configurations:
      command: cmake -B build
      options:
        - name: FEATURE
          enabled_format: FEATURE=ON
        - name: CMAKE_C_FLAGS
          enabled_value:
            default: -O2
)");

        YAML::Node user_config = gen_user_config({"application"}, false, "system");
        find_option(user_config["kez"]["application"]["build"]["configurations"]["options"],
                    "CMAKE_CXX_FLAGS")["enabled_value"] = "-Og ${library.incflags}";

        const BashCommandPlan plan = parse_user_config(user_config, settings());

        ASSERT_EQ(plan.size(), 1U);
        ASSERT_EQ(plan[0].commands.size(), 2U);
        EXPECT_EQ(plan[0].commands[0], "echo -I/opt/env/library/include -L/opt/env/library/lib64 "
                                       "-Wl,-rpath,/opt/env/library/lib64");
        const std::string& command = plan[0].commands[1];
        EXPECT_NE(command.find("-DFEATURE=ON"), std::string::npos);
        EXPECT_NE(command.find("-DCMAKE_INSTALL_PREFIX=\"/opt/env/application\""),
                  std::string::npos);
        EXPECT_NE(command.find("-DCMAKE_C_FLAGS=\"-O2\""), std::string::npos);
        EXPECT_EQ(command.find("-DCMAKE_C_FLAGS=\"-I/opt/env/include\""), std::string::npos);
        EXPECT_NE(command.find("-DCMAKE_CXX_FLAGS=\"-Og -I/opt/env/library/include\""),
                  std::string::npos);
        EXPECT_NE(command.find("-DCMAKE_EXE_LINKER_FLAGS=\"-L/opt/env/library/lib64 "
                               "-Wl,-rpath,/opt/env/library/lib64 -lexample\""),
                  std::string::npos);
        EXPECT_NE(command.find("-DCMAKE_BUILD_RPATH=\"/opt/env/library/lib64\""),
                  std::string::npos);
    }

    TEST_F(TemporaryUserConfigParserDatabase, UsesCompilerSpecificAutotoolsLinkerFlags) {
        write_package("nvhpc-compilers", R"(
recipe:
  name: nvhpc-compilers
  type: compiler
  properties:
    c: ${compiler.prefix}/bin/nvc
    cxx: ${compiler.prefix}/bin/nvc++
    fort: ${compiler.prefix}/bin/nvfortran
    lib: ${compiler.prefix}/lib
)");
        write_package("library", R"(
recipe:
  name: library
  type: package
  properties:
    include: ${library.prefix}/include
    lib: ${library.prefix}/lib
    libs: -lexample
)");
        write_package("application", R"(
recipe:
  name: application
  type: package
  toolchain: autotools
  dependencies: [library]
  build:
    configurations:
      options:
        - name: enable-feature
)");

        const YAML::Node user_config =
            gen_user_config({"application"}, false, "nvhpc-compilers@1.0");

        const BashCommandPlan plan = parse_user_config(user_config, settings());

        ASSERT_EQ(plan.size(), 1U);
        ASSERT_EQ(plan[0].commands.size(), 1U);
        const std::string& command = plan[0].commands[0];
        EXPECT_NE(command.find("./configure --enable-feature"), std::string::npos);
        EXPECT_NE(command.find("CC=\"/opt/compilers/nvhpc-compilers-1.0/nvhpc-compilers/bin/nvc\""),
                  std::string::npos);
        EXPECT_NE(
            command.find("CXXFLAGS=\"-O3 -march=native -mtune=native -I/opt/env/library/include\""),
            std::string::npos);
        EXPECT_NE(
            command.find("LDFLAGS=\"-L/opt/compilers/nvhpc-compilers-1.0/nvhpc-compilers/lib "
                         "-Xlinker -rpath,/opt/compilers/nvhpc-compilers-1.0/nvhpc-compilers/lib "
                         "-L/opt/env/library/lib -Xlinker -rpath,/opt/env/library/lib\""),
            std::string::npos);
        EXPECT_EQ(command.find("LIBS="), std::string::npos);
    }

    TEST_F(TemporaryUserConfigParserDatabase,
           KeepsSystemBuildCompilerSeparateFromSameNamedCompilerTarget) {
        write_package("gcc", R"(
recipe:
  name: gcc
  type: compiler
  toolchain: autotools
  source:
    type: tarball
    releases:
      - version: 16.0
        url: https://example.invalid/gcc-16.0.tar.gz
  build:
    configurations: {}
  properties:
    c: ${gcc.prefix}/bin/gcc
    cxx: ${gcc.prefix}/bin/g++
    fort: ${gcc.prefix}/bin/gfortran
    lib: ${gcc.prefix}/lib64
)");
        clear_db_cache();

        const YAML::Node user_config = gen_user_config({"gcc"}, false, "system", {{"gcc", "16.0"}});
        const BashCommandPlan plan   = parse_user_config(user_config, settings());

        ASSERT_EQ(plan.size(), 1U);
        const auto configure = std::find_if(
            plan[0].commands.begin(), plan[0].commands.end(),
            [](const std::string& command) { return command.rfind("./configure", 0) == 0; });
        ASSERT_NE(configure, plan[0].commands.end());
        EXPECT_NE(configure->find("--prefix=\"/opt/compilers/gcc-16.0/gcc\""), std::string::npos);
        EXPECT_NE(configure->find("CC=\"/opt/system/bin/gcc\""), std::string::npos);
        EXPECT_EQ(configure->find("CC=\"/opt/compilers/gcc-16.0/gcc/bin/gcc\""), std::string::npos);
    }

    TEST_F(TemporaryUserConfigParserDatabase,
           KeepsOlderBuildCompilerSeparateFromSameNamedCompilerTarget) {
        write_package("gcc", R"(
recipe:
  name: gcc
  type: compiler
  toolchain: autotools
  source:
    type: tarball
    releases:
      - version: 16.0
        url: https://example.invalid/gcc-16.0.tar.gz
      - version: 15.0
        url: https://example.invalid/gcc-15.0.tar.gz
  build:
    configurations: {}
  properties:
    c: ${gcc.prefix}/bin/gcc
    cxx: ${gcc.prefix}/bin/g++
    fort: ${gcc.prefix}/bin/gfortran
    lib: ${gcc.prefix}/lib64
)");
        clear_db_cache();

        const YAML::Node user_config =
            gen_user_config({"gcc"}, false, "gcc@15.0", {{"gcc", "16.0"}});
        const BashCommandPlan plan = parse_user_config(user_config, settings());

        ASSERT_EQ(plan.size(), 1U);
        const auto configure = std::find_if(
            plan[0].commands.begin(), plan[0].commands.end(),
            [](const std::string& command) { return command.rfind("./configure", 0) == 0; });
        ASSERT_NE(configure, plan[0].commands.end());
        EXPECT_NE(configure->find("--prefix=\"/opt/compilers/gcc-16.0/gcc\""), std::string::npos);
        EXPECT_NE(configure->find("CC=\"/opt/compilers/gcc-15.0/gcc/bin/gcc\""), std::string::npos);
        EXPECT_EQ(configure->find("CC=\"/opt/compilers/gcc-16.0/gcc/bin/gcc\""), std::string::npos);
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
kez:
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
        EXPECT_EQ(plan[0].commands, std::vector<std::string>({"echo /opt/env/demo"}));
    }

    TEST_F(TemporaryUserConfigParserDatabase,
           UsesRenamedInstallRootForManagedTargetButKeepsPackageVersion) {
        write_package("openmpi", R"(
recipe:
  name: openmpi
  type: mpi
  source:
    type: tarball
    releases:
      - version: 5.0.10
        url: https://example.invalid/openmpi.tar.gz
  build:
    configurations:
      command: echo ${openmpi.prefix} ${openmpi.version}
)");
        const YAML::Node user_config                          = YAML::Load(R"(
kez:
  openmpi:
    version: 5.0.10
    compiler: system
    build: {}
recipe:
  abstract_packages: {}
  dependencies: [openmpi]
  targets: [openmpi]
)");
        UserConfigParserSettings parser_settings              = settings();
        parser_settings.install_prefix                        = "/opt/mpis/openmpi-nohcoll-system";
        parser_settings.use_install_prefix_for_managed_target = true;

        const BashCommandPlan plan = parse_user_config(user_config, parser_settings);

        ASSERT_EQ(plan.size(), 1U);
        EXPECT_NE(std::find(plan[0].commands.begin(), plan[0].commands.end(),
                            "echo /opt/mpis/openmpi-nohcoll-system/openmpi 5.0.10"),
                  plan[0].commands.end());
    }

    TEST_F(TemporaryUserConfigParserDatabase, ResolvesForwardConditionReferencesToOptionStates) {
        write_package("helper", R"(
recipe:
  name: helper
  type: package
  build:
    configurations:
      options:
        - name: use_cuda
          user_configurable: true
          enabled:
            default: false
          enabled_format: ""
)");
        write_package("application", R"(
recipe:
  name: application
  type: package
  dependencies: [helper]
  build:
    configurations:
      command: configure
      options:
        - name: FLAGS
          enabled_value:
            default: base
            conditions:
              - condition: ${helper.config.use_cuda} true
                action: append
                value: cuda
)");

        const YAML::Node user_config = YAML::Load(R"(
kez:
  application:
    compiler: system
  helper:
    compiler: system
    build:
      configurations:
        options:
          - name: use_cuda
            enabled: true
            enabled_value: ~
            disabled_value: ~
recipe:
  abstract_packages: {}
  dependencies: [application, helper]
  targets: [application]
)");

        const BashCommandPlan plan = parse_user_config(user_config, settings());

        ASSERT_EQ(plan.size(), 1U);
        EXPECT_EQ(plan[0].package, "application");
        ASSERT_EQ(plan[0].commands.size(), 1U);
        EXPECT_EQ(plan[0].commands[0], "configure FLAGS=\"base cuda\"");
    }

    TEST_F(TemporaryUserConfigParserDatabase,
           EvaluatesLogicalEnvironmentVersionAndRequirementConditionsAfterComponentSplit) {
        write_package("helper", R"(
recipe:
  name: helper
  type: package
)");
        write_package("application", R"(
recipe:
  name: application
  type: package
  dependencies: [helper]
  build:
    configurations:
      command: configure
      environment:
        - name: KEZ_CONDITION_MARK
          user_configurable: true
          default: ~
      options:
        - name: feature
          user_configurable: true
          enabled:
            default: false
          enabled_format: ""
        - name: FLAGS
          enabled_value:
            default: base
            conditions:
              - condition: environment ${application.env.KEZ_CONDITION_MARK}
                action: append
                value: environment
              - condition: version ${application.version}>=2.0,<3.0
                action: append
                value: version
              - condition: not (${application.config.feature} false || false)
                action: append
                value: logical
              - condition: required helper && true
                action: append
                value: required
)");

        const YAML::Node user_config = YAML::Load(R"(
kez:
  application:
    version: 2.5
    compiler: system
    build:
      configurations:
        environment:
          - name: KEZ_CONDITION_MARK
            value: present
        options:
          - name: feature
            enabled: true
            enabled_value: ~
            disabled_value: ~
  helper:
    compiler: system
recipe:
  abstract_packages: {}
  dependencies: [application, helper]
  targets: [application]
)");

        unsetenv("KEZ_CONDITION_MARK");
        const BashCommandPlan plan = parse_user_config(user_config, settings());

        ASSERT_EQ(plan.size(), 1U);
        EXPECT_EQ(plan[0].commands,
                  std::vector<std::string>(
                      {"export KEZ_CONDITION_MARK=\"present\"",
                       "configure FLAGS=\"base environment version logical required\"",
                       "unset KEZ_CONDITION_MARK"}));
    }

    TEST_F(TemporaryUserConfigParserDatabase, RejectsInvalidConfigOptionConditionNames) {
        write_package("helper", R"(
recipe:
  name: helper
  type: package
  build:
    configurations:
      options:
        - name: use_feature
          user_configurable: true
          enabled:
            default: false
          enabled_format: ""
)");
        write_package("application", R"(
recipe:
  name: application
  type: package
  dependencies: [helper]
  build:
    configurations:
      command: configure
      options:
        - name: FLAGS
          enabled_value:
            default: base
            conditions:
              - condition: ${helper.config.use-feature} true
                action: append
                value: typo
)");

        const YAML::Node user_config                   = YAML::Load(R"(
kez:
  application:
    compiler: system
  helper:
    compiler: system
    build:
      configurations:
        options:
          - name: use_feature
            enabled: true
            enabled_value: ~
            disabled_value: ~
recipe:
  abstract_packages: {}
  dependencies: [application, helper]
  targets: [application]
)");
        const UserConfigParserSettings parser_settings = settings();

        EXPECT_EXIT(static_cast<void>(parse_user_config(user_config, parser_settings)),
                    ::testing::ExitedWithCode(EXIT_FAILURE),
                    "condition references unresolved option 'helper.config.use-feature'");
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

    TEST_F(TemporaryUserConfigParserDatabase,
           SchedulesBuildableDependenciesReachableThroughPropertyOnlyFacade) {
        const std::filesystem::path source = KEZ_SOURCE_DIR;
        setenv("KEZ_DB", (source / "database").c_str(), 1);
        setenv("KEZ_HOME", source.c_str(), 1);
        clear_db_cache();

        // llamacpp -> {cuda, nvhpc-nccl}, and nvhpc-nccl -> nvhpc. nvhpc-nccl is a
        // property-only facade (no source/build) so it emits no commands and is absent
        // from the plan; llamacpp must still wait on nvhpc, the buildable package the
        // facade depends on, so that parallel installation orders nvhpc before llamacpp.
        const YAML::Node user_config             = gen_user_config({"llamacpp"}, false, "system");
        UserConfigParserSettings parser_settings = settings();
        parser_settings.kez_home                 = source;
        const BashCommandPlan plan               = parse_user_config(user_config, parser_settings);

        std::vector<std::string> plan_packages;
        std::vector<std::string> llamacpp_dependencies;
        for (const PackageCommands& package : plan) {
            plan_packages.push_back(package.package);
            if (package.package == "llamacpp") {
                llamacpp_dependencies = package.dependencies;
            }
        }

        ASSERT_NE(std::find(plan_packages.begin(), plan_packages.end(), "nvhpc"),
                  plan_packages.end())
            << "nvhpc must be built as part of the llamacpp plan";
        ASSERT_NE(std::find(plan_packages.begin(), plan_packages.end(), "llamacpp"),
                  plan_packages.end())
            << "llamacpp must be built as part of the plan";
        // The facade itself produces no commands and must not appear as a plan package.
        EXPECT_EQ(std::find(plan_packages.begin(), plan_packages.end(), "nvhpc-nccl"),
                  plan_packages.end())
            << "nvhpc-nccl is a property-only facade and should not be a plan package";
        EXPECT_NE(std::find(llamacpp_dependencies.begin(), llamacpp_dependencies.end(), "cuda"),
                  llamacpp_dependencies.end())
            << "llamacpp must depend on cuda";
        EXPECT_NE(std::find(llamacpp_dependencies.begin(), llamacpp_dependencies.end(), "nvhpc"),
                  llamacpp_dependencies.end())
            << "llamacpp must depend on nvhpc transitively via the nvhpc-nccl facade";
    }

    TEST_F(TemporaryUserConfigParserDatabase,
           RebuildSetIncludesDependentsReachedThroughPropertyOnlyFacade) {
        const std::filesystem::path source = KEZ_SOURCE_DIR;
        setenv("KEZ_DB", (source / "database").c_str(), 1);
        setenv("KEZ_HOME", source.c_str(), 1);
        clear_db_cache();

        const YAML::Node user_config             = gen_user_config({"llamacpp"}, false, "system");
        UserConfigParserSettings parser_settings = settings();
        parser_settings.kez_home                 = source;
        const BashCommandPlan plan               = parse_user_config(user_config, parser_settings);

        // Rebuilding nvhpc must rebuild llamacpp, which reaches nvhpc only through the
        // nvhpc-nccl facade. Without the transitive scheduling edge, nvhpc would have no
        // dependents in the plan and llamacpp would be wrongly omitted from the rebuild set.
        const std::vector<std::string> rebuild_set = compute_rebuild_set(plan, "nvhpc");
        EXPECT_NE(std::find(rebuild_set.begin(), rebuild_set.end(), "nvhpc"), rebuild_set.end())
            << "the rebuilt package must be in its own rebuild set";
        EXPECT_NE(std::find(rebuild_set.begin(), rebuild_set.end(), "llamacpp"), rebuild_set.end())
            << "llamacpp must be rebuilt when nvhpc changes";
        EXPECT_EQ(std::find(rebuild_set.begin(), rebuild_set.end(), "cuda"), rebuild_set.end())
            << "cuda is independent of nvhpc and must not be rebuilt";
    }

    TEST_F(TemporaryUserConfigParserDatabase, ResolvesVersionMajorFromSourceRelease) {
        write_package("mypkg", R"(
recipe:
  name: mypkg
  type: package
  source:
    type: tarball
    releases:
      - version: 2.0.0
        url: https://example.invalid/mypkg.tar.gz
  build:
    configurations:
      command: echo ${mypkg.version.major}
)");
        const YAML::Node user_config = YAML::Load(R"(
kez:
  mypkg:
    version: 2.0.0
    compiler: system
    build: {}
recipe:
  abstract_packages: {}
  dependencies: [mypkg]
  targets: [mypkg]
)");

        const BashCommandPlan plan = parse_user_config(user_config, settings());

        ASSERT_EQ(plan.size(), 1U);
        EXPECT_NE(std::find(plan[0].commands.begin(), plan[0].commands.end(), "echo 2"),
                  plan[0].commands.end());
    }

    TEST_F(TemporaryUserConfigParserDatabase, ResolvesVersionMajorWithSingleComponentVersion) {
        write_package("mypkg", R"(
recipe:
  name: mypkg
  type: package
  source:
    type: tarball
    releases:
      - version: latest
        url: https://example.invalid/mypkg.tar.gz
  build:
    configurations:
      command: echo ${mypkg.version.major}
)");
        const YAML::Node user_config = YAML::Load(R"(
kez:
  mypkg:
    version: latest
    compiler: system
    build: {}
recipe:
  abstract_packages: {}
  dependencies: [mypkg]
  targets: [mypkg]
)");

        const BashCommandPlan plan = parse_user_config(user_config, settings());

        ASSERT_EQ(plan.size(), 1U);
        EXPECT_NE(std::find(plan[0].commands.begin(), plan[0].commands.end(), "echo latest"),
                  plan[0].commands.end());
    }

}  // namespace
