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

    PackageCommands make_commands(const std::string& package,
                                  const std::vector<std::string>& dependencies) {
        PackageCommands commands;
        commands.package      = package;
        commands.dependencies = dependencies;
        commands.commands.push_back("echo " + package);
        return commands;
    }

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

    TEST_F(RebuildStateFileTest, LoadsInstalledSequence) {
        write_state("state:\n  - hdf5\n  - h5hut\n  - mpi\n");
        const std::vector<std::string> installed = load_installed_packages(path_);
        EXPECT_EQ(installed, (std::vector<std::string> {"hdf5", "h5hut", "mpi"}));
    }

    TEST_F(RebuildStateFileTest, MissingFileReturnsEmpty) {
        const std::vector<std::string> installed = load_installed_packages(path_);
        EXPECT_TRUE(installed.empty());
    }

    TEST_F(RebuildStateFileTest, EmptyStateReturnsEmpty) {
        write_state("state:\n");
        const std::vector<std::string> installed = load_installed_packages(path_);
        EXPECT_TRUE(installed.empty());
    }

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

    TEST(ComputeRebuildSet, RebuildingTopKeepsOnlyItself) {
        BashCommandPlan plan;
        plan.push_back(make_commands("a", {"b"}));
        plan.push_back(make_commands("b", {"c"}));
        plan.push_back(make_commands("c", {}));

        const std::vector<std::string> rebuild_set = compute_rebuild_set(plan, "a");
        EXPECT_EQ(rebuild_set, (std::vector<std::string> {"a"}));
    }

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
