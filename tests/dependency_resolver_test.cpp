/**
 * @file dependency_resolver_test.cpp
 * @brief Tests for the dependency resolution pipeline.
 *
 * This test suite covers the core components of dependency resolution:
 *   - Extraction of essential and optional dependencies from typed database fields.
 *   - Topological sorting of dependency graphs, including cycle detection.
 *   - Resolution of abstract packages (via advice/heuristics) and boundary
 *     package types (system, compiler) whose sub-dependencies are not admitted
 *     into the install set.
 *   - Interactive (stdin-driven) selection of optional dependencies and
 *     abstract-package implementations.
 *   - An integration test that exercises the full resolver against the real
 *     database to verify shared-implementation deduplication and ordering.
 *
 * Each test constructs isolated temporary directories for database and
 * heuristics files, and restores the original environment on teardown.
 */

#include <gtest/gtest.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <database/database.hpp>
#include <dependency_resolver/essential_dependencies.hpp>
#include <dependency_resolver/optional_dependencies.hpp>
#include <dependency_resolver/resolve_dependencies.hpp>
#include <dependency_resolver/toposort.hpp>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_set>

namespace {

    /**
     * @brief RAII helper that saves environment-variable state and restores it
     *        on destruction.
     *
     * Restores happen in reverse order of `set()` calls (stack order). A
     * variable that was unset before the first `set()` call is unset again
     * on restore; one that had a previous value is restored to that value.
     *
     * Edge cases handled:
     *   - Variables that did not previously exist are correctly unset.
     *   - Multiple overlapping saves of the same variable: each `set()`
     *     snapshots the *current* value, so the outermost restore wins.
     */
    class ScopedEnvironment {
       public:
        ~ScopedEnvironment() {
            for (auto current = previous_.rbegin(); current != previous_.rend(); ++current) {
                if (current->second.has_value()) {
                    setenv(current->first.c_str(), current->second->c_str(), 1);
                } else {
                    unsetenv(current->first.c_str());
                }
            }
        }

        void set(const std::string& name, const std::string& value) {
            const char* current = std::getenv(name.c_str());
            previous_.emplace_back(
                name, current == nullptr ? std::nullopt : std::optional<std::string>(current));
            setenv(name.c_str(), value.c_str(), 1);
        }

       private:
        std::vector<std::pair<std::string, std::optional<std::string>>> previous_;
    };

    class TemporaryResolverDatabase : public ::testing::Test {
       protected:
        void SetUp() override {
            remember_environment("KEZ_DB", previous_database_);
            remember_environment("KEZ_HOME", previous_home_);
            remember_environment("KEZ_ARCH", previous_architecture_);

            path_ = std::filesystem::temp_directory_path() /
                    ("kez-dependency-resolver-test-" + std::to_string(getpid()));
            std::filesystem::remove_all(path_);
            std::filesystem::create_directories(path_ / "database");
            std::filesystem::create_directories(path_ / "heuristics");
            setenv("KEZ_DB", (path_ / "database").c_str(), 1);
            setenv("KEZ_HOME", path_.c_str(), 1);
            setenv("KEZ_ARCH", "x86_64", 1);
            clear_db_cache();
        }

        void TearDown() override {
            clear_db_cache();
            std::filesystem::remove_all(path_);
            restore_environment("KEZ_DB", previous_database_);
            restore_environment("KEZ_HOME", previous_home_);
            restore_environment("KEZ_ARCH", previous_architecture_);
        }

        static void remember_environment(const char* name, std::optional<std::string>& previous) {
            const char* value = std::getenv(name);
            if (value != nullptr) {
                previous = value;
            }
        }

        static void restore_environment(const char* name,
                                        const std::optional<std::string>& previous) {
            if (previous.has_value()) {
                setenv(name, previous->c_str(), 1);
            } else {
                unsetenv(name);
            }
        }

        void write_package(const std::string& package, const std::string& contents) const {
            const std::filesystem::path directory = path_ / "database" / package;
            std::filesystem::create_directories(directory);
            std::ofstream output(directory / "latest.yaml");
            ASSERT_TRUE(output.good());
            output << contents;
        }

        void write_advice(const std::string& contents) const {
            std::ofstream output(path_ / "heuristics" / "advice.yaml");
            ASSERT_TRUE(output.good());
            output << contents;
        }

        std::filesystem::path path_;
        std::optional<std::string> previous_database_;
        std::optional<std::string> previous_home_;
        std::optional<std::string> previous_architecture_;
    };

    std::unordered_set<std::string> as_set(const std::vector<std::string>& values) {
        return {values.begin(), values.end()};
    }

    bool appears_before(const std::vector<std::string>& values, const std::string& first,
                        const std::string& second) {
        return std::find(values.begin(), values.end(), first) <
               std::find(values.begin(), values.end(), second);
    }

    TEST_F(TemporaryResolverDatabase, ExtractsDependenciesFromTypedDatabaseFields) {
        write_package("root", R"(
recipe:
  name: root
  type: package
  dependencies: [required]
  build:
    configurations:
      environment:
        - name: ROOT_ENV
          requires: [environment-optional, shared]
          default: value
      options:
        - name: root-option
          requires: [option-optional, shared]
    stages:
      - target: build
        configurations:
          options:
            - name: stage-option
              requires: [stage-optional]
)");

        EXPECT_EQ(get_essential_dependencies("root"), std::vector<std::string>({"required"}));
        EXPECT_EQ(get_optional_dependencies("root"),
                  std::vector<std::string>(
                      {"environment-optional", "shared", "option-optional", "stage-optional"}));
    }

    TEST(DependencyTopologicalSort, OrdersDependenciesFirstAndDetectsCycles) {
        const DependencyGraph linear = {
            {"application", {"library"}}, {"library", {"runtime"}}, {"runtime", {}}};
        EXPECT_EQ(topological_sort(linear),
                  std::vector<std::string>({"runtime", "library", "application"}));

        const DependencyGraph referenced_only = {{"application", {"library"}}};
        EXPECT_EQ(topological_sort(referenced_only),
                  std::vector<std::string>({"library", "application"}));

        const DependencyGraph cyclic = {{"a", {"b"}}, {"b", {"c"}}, {"c", {"a"}}};
        EXPECT_EXIT(static_cast<void>(topological_sort(cyclic)),
                    ::testing::ExitedWithCode(EXIT_FAILURE),
                    "Dependency cycle detected: a -> b -> c -> a");
    }

    TEST_F(TemporaryResolverDatabase, ResolvesAbstractAndBoundaryPackagesWithoutLeakingState) {
        write_advice(R"(
advice:
  abstract-api:
    x86_64: implementation
)");
        write_package("application", R"(
recipe:
  name: application
  type: package
  dependencies: [required, abstract-api, system-library, compiler]
  build:
    configurations:
      options:
        - name: optional-feature
          requires: [optional-library]
)");
        write_package("required", R"(
recipe:
  name: required
  type: package
  dependencies: [transitive]
)");
        write_package("transitive", "recipe: {name: transitive, type: package}\n");
        write_package("abstract-api", R"(
recipe:
  name: abstract-api
  type: abstract
  implementations: [implementation]
)");
        write_package("implementation", R"(
recipe:
  name: implementation
  type: package
  dependencies: [implementation-child]
)");
        write_package("implementation-child",
                      "recipe: {name: implementation-child, type: package}\n");
        write_package("system-library", R"(
recipe:
  name: system-library
  type: system
  dependencies: [ignored-system-child]
)");
        write_package("compiler", R"(
recipe:
  name: compiler
  type: compiler
  dependencies: [compiler-core]
)");
        write_package("compiler-core", "recipe: {name: compiler-core, type: package}\n");
        write_package("standalone", "recipe: {name: standalone, type: package}\n");

        testing::internal::CaptureStdout();
        const DependencyResolution result = resolve_dependencies({"application"}, false);
        const std::string output          = testing::internal::GetCapturedStdout();

        const std::unordered_set<std::string> expected_all = {
            "application",          "required",       "transitive", "implementation",
            "implementation-child", "system-library", "compiler"};
        EXPECT_EQ(as_set(result.first.first), expected_all);
        EXPECT_EQ(as_set(result.first.second),
                  std::unordered_set<std::string>({"application", "required", "transitive",
                                                   "implementation", "implementation-child",
                                                   "compiler"}));
        EXPECT_EQ(result.second.at("abstract-api"), "implementation");
        EXPECT_TRUE(appears_before(result.first.first, "application", "required"));
        EXPECT_TRUE(appears_before(result.first.first, "required", "transitive"));
        EXPECT_TRUE(appears_before(result.first.first, "implementation", "implementation-child"));
        EXPECT_EQ(
            result.first.first.end(),
            std::find(result.first.first.begin(), result.first.first.end(), "optional-library"));
        EXPECT_EQ(result.first.first.end(),
                  std::find(result.first.first.begin(), result.first.first.end(),
                            "ignored-system-child"));
        EXPECT_EQ(result.first.first.end(),
                  std::find(result.first.first.begin(), result.first.first.end(), "compiler-core"));
        EXPECT_NE(output.find("Selected implementation for 'abstract-api': implementation"),
                  std::string::npos);
        EXPECT_NE(output.find("Exclude optional dependency: optional-library"), std::string::npos);

        const DependencyResolution second = resolve_dependencies({"standalone"}, false);
        EXPECT_EQ(second.first.first, std::vector<std::string>({"standalone"}));
        EXPECT_TRUE(second.second.empty());

        const DependencyResolution compiler = resolve_dependencies({"compiler"}, false);
        EXPECT_EQ(compiler.first.first, std::vector<std::string>({"compiler", "compiler-core"}));
    }

    TEST_F(TemporaryResolverDatabase, InteractiveModeSelectsOptionalAndAbstractPackages) {
        write_package("application", R"(
recipe:
  name: application
  type: package
  dependencies: [abstract-api]
  build:
    configurations:
      options:
        - name: optional-feature
          requires: [optional-library]
)");
        write_package("abstract-api", R"(
recipe:
  name: abstract-api
  type: abstract
  implementations: [implementation]
)");
        write_package("implementation", "recipe: {name: implementation, type: package}\n");
        write_package("optional-library", "recipe: {name: optional-library, type: package}\n");

        std::istringstream input("y\n implementation \n");
        std::streambuf* previous_input = std::cin.rdbuf(input.rdbuf());
        testing::internal::CaptureStdout();
        const DependencyResolution result = resolve_dependencies({"application"}, true);
        static_cast<void>(testing::internal::GetCapturedStdout());
        std::cin.rdbuf(previous_input);

        EXPECT_EQ(
            as_set(result.first.first),
            std::unordered_set<std::string>({"application", "implementation", "optional-library"}));
        EXPECT_EQ(result.second.at("abstract-api"), "implementation");
    }

    TEST(DependencyResolverIntegration, ResolvesARepositoryPackageWithSharedImplementations) {
        ScopedEnvironment environment;
        environment.set("KEZ_DB", (std::filesystem::path(KEZ_SOURCE_DIR) / "database").string());
        environment.set("KEZ_HOME", KEZ_SOURCE_DIR);
        environment.set("KEZ_ARCH", "x86_64");
        clear_db_cache();

        testing::internal::CaptureStdout();
        const DependencyResolution result = resolve_dependencies({"conquest"}, false);
        static_cast<void>(testing::internal::GetCapturedStdout());
        clear_db_cache();

        EXPECT_EQ(result.second.at("blas"), "intel-oneapi-mkl");
        EXPECT_EQ(result.second.at("lapack"), "intel-oneapi-mkl");
        EXPECT_EQ(result.second.at("fftw3-api"), "intel-oneapi-mkl");
        EXPECT_EQ(result.second.at("scalapack"), "intel-oneapi-mkl");
        EXPECT_EQ(result.second.at("mpi"), "openmpi");
        EXPECT_EQ(
            std::count(result.first.first.begin(), result.first.first.end(), "intel-oneapi-mkl"),
            1);
        EXPECT_TRUE(appears_before(result.first.first, "conquest", "intel-oneapi-mkl"));
    }

}  // namespace
