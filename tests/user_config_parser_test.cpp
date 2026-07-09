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
#include <parser/user_config_parser.hpp>
#include <rebuild/rebuild.hpp>
#include <string>
#include <user_config_generator/user_config_generator.hpp>
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
            std::filesystem::create_directories(path_ / "patches" / "application");
            setenv("KEZ_DB", (path_ / "database").c_str(), 1);
            setenv("KEZ_HOME", path_.c_str(), 1);
            setenv("KEZ_ARCH", "x86_64", 1);
            setenv("CFLAGS", "host flags", 1);
            write_package("gcc", R"(
recipe:
  name: gcc
  type: compiler
  properties:
    c: /usr/bin/gcc
    cxx: /usr/bin/g++
    fort: /usr/bin/gfortran
)");
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

        /** @brief Restore or unset an environment variable that was saved before the test. */
        static void restore(const char* name, const std::optional<std::string>& value) {
            if (value.has_value()) {
                setenv(name, value->c_str(), 1);
            } else {
                unsetenv(name);
            }
        }

        /**
         * @brief Write a package recipe into the temporary database under
         *        `database/<package>/latest.yaml`.
         */
        void write_package(const std::string& package, const std::string& contents) const {
            write_package(package, "latest.yaml", contents);
        }

        /**
         * @brief Write a package recipe into the temporary database under
         *        `database/<package>/<filename>`.
         *
         * This allows tests to create version-range recipes (e.g. "1.0-1.9.yaml")
         * alongside the default latest.yaml.
         */
        void write_package(const std::string& package, const std::string& filename,
                           const std::string& contents) const {
            const std::filesystem::path directory = path_ / "database" / package;
            std::filesystem::create_directories(directory);
            std::ofstream output(directory / filename);
            ASSERT_TRUE(output.good());
            output << contents;
        }

        /** @brief Write an arbitrary file under the temporary root directory. */
        void write_file(const std::filesystem::path& path, const std::string& contents) const {
            std::ofstream output(path);
            ASSERT_TRUE(output.good());
            output << contents;
        }

        /**
         * @brief Return a fully populated UserConfigParserSettings pointing into
         *        the temporary directory.
         *
         * All prefix paths are set to well-known locations under /opt so that
         * the expected command strings in assertions are deterministic and
         * independent of the host filesystem layout.
         */
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

    /**
     * @brief Search a YAML sequence of option nodes for one whose "name"
     *        matches @p name and return it.
     *
     * Fails the current test via ADD_FAILURE if no matching option is found.
     * Used by tests to surgically modify a single option in the generated
     * user config before parsing.
     *
     * @param options  YAML node representing a sequence of option objects.
     * @param name     The "name" field to search for.
     * @return The matching YAML node (empty node on failure, but the test
     *         already failed).
     */
    YAML::Node find_option(YAML::Node options, const std::string& name) {
        for (YAML::Node option : options) {
            if (option["name"].as<std::string>() == name) {
                return option;
            }
        }
        ADD_FAILURE() << "Missing option: " << name;
        return YAML::Node();
    }

    /**
     * @brief Verify that the parser emits dependency-ordered commands for
     *        typed recipes and correctly renders user-supplied configuration
     *        values.
     *
     * Sets up two packages ("library" with a git source, "application" with
     * a tarball source, cmake toolchain, patches, and build
     * configurations).  The test enables a patch, overrides environment and
     * option values in the user config, then parses it and checks:
     *   - The plan contains two entries, ordered by dependency (library
     *     before application).
     *   - Application commands include wget/download, unpack, cache,
     *     patch application, preprocessing, environment export, cmake
     *     configure (with FEATURE=ON, merged linker flags from properties
     *     and overrides), build, install, and postprocessing.
     *   - The library commands are the shallow-clone / cache / make steps.
     *   - When the user config specifies a local source path (version
     *     starting with "dev@"), the parser substitutes a file copy for
     *     the download step.
     *
     * Edge cases covered:
     *   - Tarball vs. git source types.
     *   - Option conditions that trigger property overrides across packages.
     *   - Template expansion of ${kez.arch}, ${application.prefix}, etc.
     *   - Local (dev@) source substitution.
     *   - Patch application ordering.
     *   - Environment variable merging (CFLAGS with user value).
     *   - CMake generator expressions with compiler properties.
     */
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
                  "git apply '" + (path_ / "patches" / "application" / "fix.patch").string() + "'");
        EXPECT_EQ(commands[7], "prepare /opt/env/.tmp/application/source");
        EXPECT_EQ(commands[8], "export CFLAGS=\"-O2 ${PATH}\"");
        EXPECT_EQ(commands[9], "cmake -B build -DFEATURE=ON "
                               "-DLINK_FLAGS=\"-lbase -lfeature -loverride\" "
                               "-DCMAKE_INSTALL_PREFIX=\"/opt/env/application\" "
                               "-DCMAKE_BUILD_TYPE=\"Release\" -DCMAKE_C_COMPILER=\"/usr/bin/gcc\" "
                               "-DCMAKE_CXX_COMPILER=\"/usr/bin/g++\" "
                               "-DCMAKE_Fortran_COMPILER=\"/usr/bin/gfortran\" "
                               "-DCMAKE_C_FLAGS=\"-O3\" -DCMAKE_CXX_FLAGS=\"-O3\" "
                               "-DCMAKE_Fortran_FLAGS=\"-O3\" -DCMAKE_CUDA_FLAGS=\"-O3\" "
                               "-DCMAKE_EXE_LINKER_FLAGS=\"-lbase -lfeature -loverride\" "
                               "-DCMAKE_SHARED_LINKER_FLAGS=\"-lbase -lfeature -loverride\" "
                               "-DCMAKE_MODULE_LINKER_FLAGS=\"-lbase -lfeature -loverride\"");
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

    /**
     * @brief Verify that abstract package properties are resolved to their
     *        concrete implementation and that option selection conditions
     *        referencing those properties work correctly.
     *
     * Sets up an abstract "mpi" package with a single implementation, an
     * "implementation" package with a compiler property, and an
     * "application" that references `${mpi.c}` and has a conditional option
     * `${mpi.use-implementation}`.  The user config maps the abstract `mpi`
     * to `implementation` and disables the `--with-mpi` option (since the
     * condition `${mpi.use-implementation} true` is false for an abstract
     * package's `use-implementation` property).
     *
     * Verifies that the plan produces a single configure command resolving
     * CC to the implementation's mpicc path and omitting `--with-mpi`.
     *
     * Edge cases covered:
     *   - Abstract-to-concrete package resolution.
     *   - Conditional option enabled/disabled by abstract property state.
     *   - Template resolution of ${mpi.c} through the implementation.
     *   - Condition using the template-style property reference syntax.
     */
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

    /**
     * @brief Verify that raw dependency paths (include, lib, libs) are expanded
     *        in preprocessing commands and that user-specified option values
     *        override recipe defaults.
     *
     * Creates a "library" package with include/lib/libs properties and an
     * "application" that depends on it and uses a cmake toolchain.  The
     * test modifies the generated user config by overriding
     * CMAKE_CXX_FLAGS with a value containing `${library.includes}` and
     * checks that:
     *   - The preprocessing "echo" command expands
     *     ${library.includes}, ${library.ldflags}, and
     *     ${library.nvldflags} to the correct -I/-L/-rpath flags.
     *   - The cmake configure command contains the default FEATURE=ON,
     *      CMAKE_INSTALL_PREFIX, CMAKE_C_FLAGS with the recipe default -O2,
     *      and CMAKE_CXX_FLAGS with the user override -Og plus the expanded
     *      include path.
     *   - CMAKE_EXE_LINKER_FLAGS contains the library's libs and paths.
     *   - CMAKE_BUILD_RPATH references the library's lib directory.
     *   - CMAKE_C_FLAGS does NOT pick up the library's include path
     *     (only CMAKE_CXX_FLAGS was overridden).
     *
     * Edge cases covered:
     *   - Template expansion of raw property paths (includes, ldflags,
     *     nvldflags) that the database parser synthesizes from the
     *     recipe's properties.
     *   - User override of an option that uses template references in its value.
     *   - Mix of default options and user-overridden options in the same command.
     *   - Verification that default options remain unchanged when only
     *     sibling options are overridden.
     */
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
    preprocessing: echo ${library.includes} ${library.ldflags} ${library.nvldflags}
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
                    "CMAKE_CXX_FLAGS")["enabled_value"] = "-Og ${library.includes}";

        const BashCommandPlan plan = parse_user_config(user_config, settings());

        ASSERT_EQ(plan.size(), 1U);
        ASSERT_EQ(plan[0].commands.size(), 2U);
        EXPECT_EQ(plan[0].commands[0],
                  "echo -I/opt/env/library/include -L/opt/env/library/lib64 "
                  "-Wl,-rpath,/opt/env/library/lib64 "
                  "-L/opt/env/library/lib64 -Xlinker -rpath,/opt/env/library/lib64");
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

    /**
     * @brief Verify that the autotools toolchain emits compiler-specific
     *        linker flags (LDFLAGS with -Xlinker -rpath) when the selected
     *        compiler is NVHPC.
     *
     * Sets up an "nvhpc-compilers" compiler recipe with lib pointing to
     * its prefix/lib, a "library" package with include/lib/libs properties,
     * and an "application" that uses the autotools toolchain and depends on
     * the library.  The user config selects "nvhpc-compilers@1.0" as the
     * compiler.
     *
     * Verifies that:
     *   - The configure command includes --enable-feature.
     *   - CC is resolved to the NVHPC compiler binary.
     *   - CXXFLAGS includes -O3 and the library's include path.
     *   - LDFLAGS includes both the compiler's lib path and the library's
     *     lib path with -Xlinker -rpath (NVHPC-specific linker syntax).
     *   - LIBS is NOT present (autotools handles -lexample via LDFLAGS
     *     differently from cmake).
     *
     * Edge cases covered:
     *   - NVHPC (non-GCC) compiler selection.
     *   - Autotools toolchain linker flag format for NVHPC.
     *   - Dual rpath entries (compiler lib + dependency lib).
     *   - Compiler property path template resolution.
     */
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
        EXPECT_NE(command.find("CC=\"/opt/compilers/nvhpc-compilers-1.0/bin/nvc\""),
                  std::string::npos);
        EXPECT_NE(command.find("CXXFLAGS=\"-O3 -I/opt/env/library/include\""), std::string::npos);
        EXPECT_NE(command.find("LDFLAGS=\"-L/opt/compilers/nvhpc-compilers-1.0/lib "
                               "-Xlinker -rpath,/opt/compilers/nvhpc-compilers-1.0/lib "
                               "-L/opt/env/library/lib -Xlinker -rpath,/opt/env/library/lib\""),
                  std::string::npos);
        EXPECT_EQ(command.find("LIBS="), std::string::npos);
    }

    /**
     * @brief Verify that the parser selects a recipe from a version-range
     *        file and resolves its canonical name.
     *
     * Creates two recipe files for "demo": a default latest.yaml (name:
     * "demo") and a version-range file "1.0-1.9.yaml" (name: "demo-v1").
     * The user config requests version "1.5", which falls within the range.
     *
     * Verifies that:
     *   - The plan contains one entry for "demo" (the original package
     *     name, not "demo-v1").
     *   - The command expands ${demo-v1.prefix} to "/opt/env/demo".
     *
     * Edge cases covered:
     *   - Version-range recipe selection (version 1.5 within 1.0-1.9).
     *   - Canonical name resolution: the plan uses the package key from
     *     the user config, not the recipe's internal name.
     *   - Template expansion using the version-range recipe's name
     *     (demo-v1) resolves correctly.
     */
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

    /**
     * @brief Verify that a package's option conditions can reference option
     *        states in another package (forward reference).
     *
     * Creates a "helper" package with a `use_cuda` option and an
     * "application" that depends on helper and whose `FLAGS` option has a
     * condition referencing `${helper.config.use_cuda}`.  The user config
     * enables `use_cuda` in the helper package.
     *
     * Verifies that the condition evaluates to true and the configure
     * command includes `FLAGS="base cuda"`.
     *
     * Edge cases covered:
     *   - Forward cross-package option condition references
     *     (`${helper.config.use_cuda}`).
     *   - Condition with action "append" adding a value to a default.
     *   - Option whose enabled_format is "" (empty format) — no format
     *     string substitution, just boolean state.
     *   - User config explicitly setting enabled_value/disabled_value to
     *     null (~) to suppress format output.
     */
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

    /**
     * @brief Verify that the parser terminates with an error when a condition
     *        references a nonexistent option in another package.
     *
     * Similar setup to ResolvesForwardConditionReferencesToOptionStates
     * but the application recipe's condition uses "use-feature" (with a
     * hyphen) while the helper package actually defines "use_feature"
     * (with underscore) — a deliberate mismatch.
     *
     * Verifies that parse_user_config() calls exit(EXIT_FAILURE) and
     * prints an error message containing "condition references unresolved
     * option 'helper.config.use-feature'".
     *
     * Edge cases covered:
     *   - Invalid cross-package option reference (name mismatch).
     *   - Error termination via exit() rather than throwing an exception
     *     (consistent with the project's no-exceptions policy).
     *   - Error message includes the full unresolved reference path.
     */
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

    /**
     * @brief Verify end-to-end parsing of real repository recipes (fio and
     *        dftbplus), checking dependency ordering and complete template
     *        resolution.
     *
     * This test switches the KEZ_DB to the project's real database directory
     * and KEZ_HOME to the source root.  It performs two rounds:
     *
     *   Round 1 — fio:
     *     Generates a config for "fio", parses it, and verifies:
     *       - The plan has two entries ordered by dependency (libaio before fio).
     *       - Every command in both packages is free of unresolved
     *         "${fio." or "${libaio." template references.
     *
     *   Round 2 — dftbplus:
     *     Generates a config for "dftbplus", marks "rdma-core" as an
     *     external package (present on the host system), parses, and
     *     verifies:
     *       - The plan has at least 3 entries.
     *       - Every command in every package is free of any "${...}"
     *         pattern containing a dot — i.e., all
     *         package-scoped property templates have been resolved.
     *
     * Edge cases covered:
     *   - Real (not synthetic) package recipes with complex dependency trees.
     *   - Template resolution across transitive dependencies.
     *   - External package declaration (packages provided by the host OS
     *     that satisfy a dependency without building).
     *   - No-op (property-only) packages that do not appear in the plan.
     *   - Verification that no dangling template references remain after
     *     parsing.
     */
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

    /**
     * @brief Verify that property-only facade packages (those with no
     *        source/build sections) are omitted from the build plan but
     *        their buildable dependencies are still scheduled.
     *
     * Uses the real database recipe for "llamacpp", which depends on
     * "nvhpc-nccl" (a property-only facade — it declares properties but
     * has no source/build), and "nvhpc-nccl" in turn depends on "nvhpc"
     * (a real compiler package that must be built).
     *
     * Verifies:
     *   - "nvhpc" appears in the plan packages (it must be built).
     *   - "llamacpp" appears in the plan packages.
     *   - "nvhpc-nccl" does NOT appear in the plan packages (it is a
     *     facade with nothing to build).
     *   - llamacpp's dependency list includes both "cuda" (a direct
     *     dependency) and "nvhpc" (reached transitively through the
     *     nvhpc-nccl facade).
     *
     * Edge cases covered:
     *   - Property-only facade elimination from the build plan.
     *   - Transitive dependency scheduling through a facade: the
     *     buildable dependency of the facade (nvhpc) must be scheduled
     *     before the dependent of the facade (llamacpp).
     *   - Parallel installation ordering: nvhpc must finish before
     *     llamacpp starts, even though the intermediate package
     *     (nvhpc-nccl) produces no commands.
     */
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

    /**
     * @brief Verify that compute_rebuild_set() includes transitive
     *        dependents reached through property-only facades.
     *
     * Uses the same real-repository llamacpp setup as
     * SchedulesBuildableDependenciesReachableThroughPropertyOnlyFacade.
     * After parsing the plan, calls compute_rebuild_set(plan, "nvhpc")
     * to simulate rebuilding the nvhpc compiler.
     *
     * Verifies:
     *   - "nvhpc" itself is in the rebuild set.
     *   - "llamacpp" is in the rebuild set (it transitively depends on
     *     nvhpc through the nvhpc-nccl facade).
     *   - "cuda" is NOT in the rebuild set (it is independent of nvhpc).
     *
     * Edge cases covered:
     *   - Rebuild-set computation through a property-only facade.
     *   - The facade (nvhpc-nccl) is absent from the plan, so the
     *     rebuild algorithm must follow the transitive scheduling edges
     *     (set during parsing) rather than the original dependency edges.
     *   - Negative check that unrelated packages are correctly excluded
     *     from the rebuild set.
     */
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

}  // namespace
