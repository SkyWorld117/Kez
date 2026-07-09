#include <gtest/gtest.h>
#include <unistd.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <parser/user_config_parser.hpp>
#include <rebuild/rebuild.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

    /**
     * @brief Constructs a minimal PackageCommands object for testing.
     *
     * Creates a PackageCommands entry with the given package name and a
     * list of dependency package names. A single placeholder command
     * ("echo <package>") is added so the result is a valid, non-empty
     * BashCommandPlan entry.
     *
     * @param package      The name of the package.
     * @param dependencies  The package names this package depends on.
     * @return A fully populated PackageCommands struct.
     */
    PackageCommands make_commands(const std::string& package,
                                  const std::vector<std::string>& dependencies) {
        PackageCommands commands;
        commands.package      = package;
        commands.dependencies = dependencies;
        commands.commands.push_back("echo " + package);
        return commands;
    }

    /**
     * @brief Test fixture for rebuild operations that read state from a
     *        temporary filesystem directory.
     *
     * On SetUp, a unique temporary directory is created (named after the
     * process PID). On TearDown, it is removed recursively. Helper method
     * write_state() writes a YAML string into a "state.yaml" file inside
     * this directory, simulating an installed-packages state file.
     *
     * Edge cases covered by the fixture:
     *   - The temporary directory is cleaned up between tests regardless of
     *     pass/fail status.
     *   - The directory path is available via the protected member path_.
     */
    class RebuildStateFileTest : public ::testing::Test {
       protected:
        void SetUp() override {
            path_ = std::filesystem::temp_directory_path() /
                    ("kez-rebuild-test-" + std::to_string(getpid()));
            std::filesystem::remove_all(path_);
            std::filesystem::create_directories(path_);
        }

        void TearDown() override { std::filesystem::remove_all(path_); }

        void write_state(const std::string& contents) {
            std::ofstream output(path_ / "state.yaml");
            output << contents;
        }

        std::filesystem::path path_;
    };

    /**
     * @brief Verifies that load_installed_packages correctly parses a
     *        non-empty state.yaml.
     *
     * Writes a state file listing three packages (hdf5, h5hut, mpi) and
     * asserts that load_installed_packages returns them in the same order.
     *
     * Edge cases covered:
     *   - Normal multi-entry state parsing.
     *   - Preservation of insertion order from the YAML sequence.
     */
    TEST_F(RebuildStateFileTest, LoadsInstalledSequence) {
        write_state("state:\n  - hdf5\n  - h5hut\n  - mpi\n");
        const std::vector<std::string> installed = load_installed_packages(path_);
        EXPECT_EQ(installed, (std::vector<std::string> {"hdf5", "h5hut", "mpi"}));
    }

    /**
     * @brief Verifies that load_installed_packages returns an empty vector
     *        when the state.yaml file does not exist.
     *
     * Because SetUp creates an empty directory, no state file is written.
     * The function must handle a missing file gracefully rather than
     * crashing.
     *
     * Edge cases covered:
     *   - Absent state file (file not found).
     */
    TEST_F(RebuildStateFileTest, MissingFileReturnsEmpty) {
        const std::vector<std::string> installed = load_installed_packages(path_);
        EXPECT_TRUE(installed.empty());
    }

    /**
     * @brief Verifies that load_installed_packages returns an empty vector
     *        when the state.yaml exists but the "state" key is empty.
     *
     * Writes a state file with an empty YAML sequence under "state:".
     * The function should return an empty vector.
     *
     * Edge cases covered:
     *   - Explicitly empty package list (zero entries).
     */
    TEST_F(RebuildStateFileTest, EmptyStateReturnsEmpty) {
        write_state("state:\n");
        const std::vector<std::string> installed = load_installed_packages(path_);
        EXPECT_TRUE(installed.empty());
    }

    /**
     * @brief Verifies that build_dependents_map inverts dependency edges
     *        correctly for a linear chain.
     *
     * Given the dependencies a -> b -> c (a depends on b, b depends on c),
     * the inverted map should contain:
     *   - b appears as a dependent of c,
     *   - a appears as a dependent of b,
     *   - a (the root) does not appear in the map because nothing depends
     *     on it.
     *
     * Edge cases covered:
     *   - Linear (non-branching) dependency chain.
     *   - Packages with no reverse edges are absent from the map rather
     *     than present with an empty vector.
     */
    TEST(BuildDependentsMap, InvertsEdges) {
        BashCommandPlan plan;
        plan.push_back(make_commands("a", {"b"}));
        plan.push_back(make_commands("b", {"c"}));
        plan.push_back(make_commands("c", {}));

        const std::unordered_map<std::string, std::vector<std::string>> dependents =
            build_dependents_map(plan);
        ASSERT_EQ(dependents.count("b"), 1u);
        EXPECT_EQ(dependents.at("b"), (std::vector<std::string> {"a"}));
        ASSERT_EQ(dependents.count("c"), 1u);
        EXPECT_EQ(dependents.at("c"), (std::vector<std::string> {"b"}));
        EXPECT_EQ(dependents.count("a"), 0u);
    }

    /**
     * @brief Verifies that compute_rebuild_set produces the transitive
     *        closure of all dependents when rebuilding a leaf package.
     *
     * Dependency chain: a -> b -> c (a depends on b, b depends on c).
     * Rebuilding c (the leaf) must include a, b, and c in the output,
     * emitted in the original plan order.
     *
     * Edge cases covered:
     *   - Leaf-node rebuild triggers full-chain reinstall.
     *   - Output ordering preserves the original plan sequence.
     */
    TEST(ComputeRebuildSet, TransitiveClosureFromLeaf) {
        // c is the leaf; b depends on c; a depends on b. Rebuilding c rebuilds all three.
        BashCommandPlan plan;
        plan.push_back(make_commands("a", {"b"}));
        plan.push_back(make_commands("b", {"c"}));
        plan.push_back(make_commands("c", {}));

        const std::vector<std::string> rebuild_set = compute_rebuild_set(plan, "c");
        // Emitted in plan order: a, b, c.
        EXPECT_EQ(rebuild_set, (std::vector<std::string> {"a", "b", "c"}));
    }

    /**
     * @brief Verifies that compute_rebuild_set returns only the target
     *        package when it has no reverse dependencies (i.e. it is the
     *        root / top-level package).
     *
     * Dependency chain: a -> b -> c. Rebuilding a, which nothing depends
     * on, should yield a singleton set containing only "a".
     *
     * Edge cases covered:
     *   - Root-node rebuild isolates a single package.
     */
    TEST(ComputeRebuildSet, RebuildingTopKeepsOnlyItself) {
        BashCommandPlan plan;
        plan.push_back(make_commands("a", {"b"}));
        plan.push_back(make_commands("b", {"c"}));
        plan.push_back(make_commands("c", {}));

        const std::vector<std::string> rebuild_set = compute_rebuild_set(plan, "a");
        EXPECT_EQ(rebuild_set, (std::vector<std::string> {"a"}));
    }

    /**
     * @brief Verifies that compute_rebuild_set correctly handles a diamond
     *        dependency pattern without duplicating packages.
     *
     * Diamond graph:
     *   d depends on b and c
     *   b depends on a
     *   c depends on a
     * Rebuilding a must include d, b, c, and a — each exactly once —
     * because both b and c transitively depend on a and d transitively
     * depends on both b and c.
     *
     * Edge cases covered:
     *   - Diamond-shaped dependency graph (diamond pattern).
     *   - Deduplication of packages reachable through multiple paths.
     *   - Preservation of plan order when multiple paths converge.
     */
    TEST(ComputeRebuildSet, DiamondClosureHasNoDuplicates) {
        // d depends on b and c; both depend on a. Rebuilding a rebuilds b, c, d once each.
        BashCommandPlan plan;
        plan.push_back(make_commands("d", {"b", "c"}));
        plan.push_back(make_commands("b", {"a"}));
        plan.push_back(make_commands("c", {"a"}));
        plan.push_back(make_commands("a", {}));

        const std::vector<std::string> rebuild_set = compute_rebuild_set(plan, "a");
        EXPECT_EQ(rebuild_set, (std::vector<std::string> {"d", "b", "c", "a"}));
    }

    /**
     * @brief Verifies that filter_plan preserves only the listed packages
     *        in their original plan order, while keeping their dependency
     *        edges intact.
     *
     * Input plan: a (depends on b), b (depends on c), c (depends on mpi).
     * Filtering for {"b", "c"} should retain only those two entries,
     * in the order they appear in the original plan (b first, then c).
     * The dependency edges are not modified: c's dependency on "mpi" is
     * kept as-is even though "mpi" is not in the filtered set — the
     * caller (install.sh) is expected to ignore edges to absent packages.
     *
     * Edge cases covered:
     *   - Filtering removes a prefix of the plan (package "a" is omitted).
     *   - Dependency edges pointing to packages outside the filtered set
     *     are preserved untouched.
     *   - The relative order of retained packages matches the original plan.
     */
    TEST(FilterPlan, KeepsOnlyListedPackagesInOrder) {
        BashCommandPlan plan;
        plan.push_back(make_commands("a", {"b"}));
        plan.push_back(make_commands("b", {"c"}));
        plan.push_back(make_commands("c", {"mpi"}));

        const BashCommandPlan filtered = filter_plan(plan, {"b", "c"});
        ASSERT_EQ(filtered.size(), 2u);
        EXPECT_EQ(filtered[0].package, "b");
        EXPECT_EQ(filtered[1].package, "c");
        // Edges are preserved untouched (install.sh ignores edges to absent packages).
        EXPECT_EQ(filtered[1].dependencies, (std::vector<std::string> {"mpi"}));
    }

}  // namespace
