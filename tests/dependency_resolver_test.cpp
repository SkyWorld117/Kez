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

    /**
     * @brief Test fixture that creates a temporary database and heuristics
     *        directory, sets the required environment variables, and cleans
     *        up on teardown.
     *
     * The fixture writes per-test package YAML recipes into a temporary
     * `database/` directory and optional advice into `heuristics/advice.yaml`.
     * Environment variables `KEZ_DB`, `KEZ_HOME`, and `KEZ_ARCH` are set so
     * the database and heuristic parsers find the ephemeral files. The
     * internal database cache is cleared before each test and after teardown
     * to prevent cross-test pollution.
     *
     * Edge cases handled:
     *   - A unique path per test (derived from PID) prevents collisions when
     *     tests run in parallel.
     *   - Previous environment values are saved and restored, so the fixture
     *     is invisible to sibling tests that rely on a real database.
     *   - The temporary directory is recursively removed even if a test
     *     assertion fails (TearDown is called unconditionally).
     */
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

        /**
         * @brief Save the current value of an environment variable into an
         *        optional string for later restoration.
         * @param name    Environment variable name.
         * @param previous Output parameter; set to std::nullopt if the
         *                 variable is unset, otherwise to its current value.
         */
        static void remember_environment(const char* name, std::optional<std::string>& previous) {
            const char* value = std::getenv(name);
            if (value != nullptr) {
                previous = value;
            }
        }

        /**
         * @brief Restore an environment variable to its saved state.
         * @param name    Environment variable name.
         * @param previous The saved state; if std::nullopt the variable is
         *                 unset, otherwise it is set to this value.
         */
        static void restore_environment(const char* name,
                                        const std::optional<std::string>& previous) {
            if (previous.has_value()) {
                setenv(name, previous->c_str(), 1);
            } else {
                unsetenv(name);
            }
        }

        /**
         * @brief Write a package recipe YAML file into the temporary database.
         * @param package  Package name (used as the directory name).
         * @param contents The complete YAML recipe content.
         *
         * The file is written to `<database>/<package>/latest.yaml`. The test
         * fails immediately (via ASSERT_TRUE) if the file cannot be opened.
         */
        void write_package(const std::string& package, const std::string& contents) const {
            const std::filesystem::path directory = path_ / "database" / package;
            std::filesystem::create_directories(directory);
            std::ofstream output(directory / "latest.yaml");
            ASSERT_TRUE(output.good());
            output << contents;
        }

        /**
         * @brief Write a heuristics advice YAML file into the temporary
         *        heuristics directory.
         * @param contents The complete advice YAML content.
         *
         * The file is written to `<heuristics>/advice.yaml`. The test fails
         * immediately if the file cannot be opened.
         */
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

    /**
     * @brief Convert a vector of strings to an unordered_set for
     *        order-independent comparison.
     * @param values The input vector.
     * @return An unordered_set containing the same elements.
     *
     * Useful when the expected set of packages is fixed but the topological
     * order of independent nodes is non-deterministic.
     */
    std::unordered_set<std::string> as_set(const std::vector<std::string>& values) {
        return {values.begin(), values.end()};
    }

    /**
     * @brief Check whether one string appears before another in a vector.
     * @param values The ordered list to check.
     * @param first  The element expected to appear earlier.
     * @param second The element expected to appear later.
     * @return true if `first` precedes `second` in `values`; false otherwise
     *         (including when either element is absent).
     *
     * Relies on std::find; if an element appears multiple times only the
     * first occurrence is considered.
     */
    bool appears_before(const std::vector<std::string>& values, const std::string& first,
                        const std::string& second) {
        return std::find(values.begin(), values.end(), first) <
               std::find(values.begin(), values.end(), second);
    }

    /**
     * @brief Verify that get_essential_dependencies and
     *        get_optional_dependencies correctly extract dependencies from
     *        every typed field in a recipe.
     *
     * The recipe declares:
     *   - An essential dependency via the top-level `dependencies` list.
     *   - Optional dependencies from environment variables' `requires` fields,
     *     build options' `requires` fields, and stage-level options'
     *     `requires` fields.
     *
     * Edge cases covered:
     *   - `shared` appears as a requirement of two different optional slots
     *     (environment + option) and must still be listed only once in the
     *     result (implicit deduplication via the returned vector).
     *   - Stage-level options are nested under a stage target and must be
     *     discovered by the optional-dependency parser.
     */
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

    /**
     * @brief Verify topological_sort produces correct install orderings and
     *        exits with an error on cycles.
     *
     * Three sub-cases:
     *   - **Linear graph**: `application` depends on `library`, which depends
     *     on `runtime`.  The result must place `runtime` first, then `library`,
     *     then `application`.
     *   - **Referenced-only node**: `library` is listed as a dependency of
     *     `application` but has no recipe entry of its own.  The sort still
     *     includes it and places it before `application`.
     *   - **Cycle**: `a -> b -> c -> a`.  The function must call
     *     exit(EXIT_FAILURE) and print a message identifying the cycle.
     *
     * Edge cases covered:
     *   - A node that only appears as a dependency (no outgoing edges) --
     *     it must still appear in the output.
     *   - A three-node cycle where every node has exactly one outgoing edge.
     *   - The error message format must match the expected diagnostic
     *     (regression guard against message changes).
     */
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

    /**
     * @brief Verify the full resolver correctly handles abstract packages,
     *        boundary package types, and state isolation across calls.
     *
     * The test database contains:
     *   - `application` with required, abstract-api, system-library, and
     *     compiler dependencies, plus an optional optional-library.
     *   - `abstract-api` (type: abstract) with an `implementation` child.
     *   - `system-library` (type: system) with an `ignored-system-child`
     *     dependency that must NOT appear in the resolved set.
     *   - `compiler` (type: compiler) with a `compiler-core` dependency
     *     that must NOT appear in the resolved set.
     *   - `standalone` -- an unrelated package resolved in a separate call.
     *
     * Expectations:
     *   - The advice file maps `abstract-api` -> `implementation` on x86_64;
     *     the resolver selects `implementation` and its transitive child
     *     `implementation-child`.
     *   - `system-library` is included but `ignored-system-child` is not.
     *   - `compiler` is included but `compiler-core` is not.
     *   - `optional-library` is absent because non-interactive mode skips
     *     optionals by default.
     *   - The output contains diagnostics for the abstract selection and
     *     the optional exclusion.
     *   - A subsequent call for `standalone` produces an empty substitution
     *     map and only the standalone node.
     *   - A subsequent call for `compiler` alone returns `compiler` and its
     *     `compiler-core` child (boundary rules apply per resolution, not
     *     globally).
     *
     * Edge cases covered:
     *   - Abstract-package substitution via heuristics advice.
     *   - Type-boundary pruning: system and compiler package children are
     *     excluded from the install set.
     *   - Transitive dependency resolution through abstract implementations.
     *   - State isolation: the resolver's internal caches must not cause
     *     results from one call to leak into a subsequent independent call.
     *   - Optional dependencies are silently excluded in non-interactive mode.
     *   - Multiple runs with overlapping packages produce consistent results.
     */
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

    /**
     * @brief Verify that interactive mode (prompt = true) allows the user to
     *        select optional dependencies and abstract-package implementations
     *        via stdin.
     *
     * The test feeds two lines into stdin:
     *   1. A confirming response ("y") to include optional dependencies.
     *   2. The name of the desired implementation ("implementation") for the
     *      abstract package.
     *
     * Edge cases covered:
     *   - Interactive selection path through the resolver is exercised
     *     (as opposed to the non-interactive default-skip path).
     *   - Stdio redirection is correctly restored after the call so that
     *     the test harness output is not corrupted.
     *   - The abstract-package substitution map is populated even when the
     *     implementation is chosen interactively rather than from heuristics.
     *   - Both optional and abstract resolution happen in a single resolver
     *     invocation.
     */
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

    /**
     * @brief Integration test that resolves a real repository package
     *        ("conquest") and verifies shared-implementation deduplication,
     *        ordering, and advice-based substitution.
     *
     * The test uses the actual database and heuristics shipped with the
     * project (via the KEZ_SOURCE_DIR macro) rather than ephemeral stubs.
     *
     * Expectations:
     *   - Abstract packages `blas`, `lapack`, `fftw3-api`, and `scalapack`
     *     are all mapped to the same concrete implementation
     *     (`intel-oneapi-mkl`).
     *   - `intel-oneapi-mkl` appears exactly once in the install order
     *     despite satisfying four abstract slots (shared implementation
     *     deduplication).
     *   - `mpi` is mapped to `openmpi` via advice.
     *   - `conquest` appears before `intel-oneapi-mkl` in the order
     *     (dependencies come before dependents).
     *
     * Edge cases covered:
     *   - A real multi-abstract-to-one-concrete mapping exercises the
     *     resolver's deduplication logic.
     *   - The topological order respects transitive dependencies through
     *     a large real graph.
     *   - The advice/heuristics engine is tested against actual YAML data
     *     rather than synthetic stubs.
     */
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
