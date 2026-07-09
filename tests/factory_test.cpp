#include <gtest/gtest.h>
#include <yaml-cpp/yaml.h>

#include <factory/factory.hpp>
#include <string>
#include <vector>

namespace {
    /**
     * @brief Verifies that factory profile inheritance and sibling references
     *        are resolved correctly.
     *
     * This test defines a YAML factory config with two buildspaces: "base"
     * (containing the profile "small" and its sibling "copy-small") and
     * "clone" (a sibling of "base"). It checks that:
     *   - parse_factory_config returns the correct number of buildspace plans.
     *   - Each profile's name, summary_regex, and command sequence are built
     *     correctly by merging inherited fields (prerun, postrun, launcher,
     *     launcher_opts, num_nodes, num_procs_per_node, cores_per_proc,
     *     summary_regex) with profile-specific overrides and template
     *     expansion (e.g. ${prerun}, ${target}).
     *   - A sibling profile ("copy-small") inherits the exact command vector
     *     of its target sibling ("small").
     *   - A buildspace-level sibling ("clone") inherits the profiles of its
     *     target buildspace ("base"), verifying cross-buildspace sibling
     *     resolution with the same command vectors.
     *
     * Edge cases covered:
     *   - Sibling references at both the profile level and the buildspace
     *     level.
     *   - Template variable expansion of inherited fields (${prerun},
     *     ${target}).
     *   - Launcher-option string construction with Slurm-style directives
     *     (--cpu-bind, OMP_NUM_THREADS, --ntasks, etc.).
     */
    TEST(FactoryParser, InheritsCommandsAndSupportsSiblings) {
        const YAML::Node config = YAML::Load(R"(
factory:
  inputs: input
  prerun: prepare
  target: run ${target}
  postrun: cleanup
  summary_regex: '^time'
  launcher: srun
  launcher_opts: --cpu-bind=cores
  num_nodes: 2
  num_procs_per_node: 4
  cores_per_proc: 3
  buildspace:
    - name: base
      prerun: "${prerun} && module list"
      profiles:
        - name: small
          target: app --small
          summary_regex: '^small'
        - name: copy-small
          sibling: small
    - name: clone
      sibling: base
)");

        const FactoryPlan plan = parse_factory_config(config);

        ASSERT_EQ(plan.size(), 2U);
        ASSERT_EQ(plan[0].profiles.size(), 2U);
        EXPECT_EQ(plan[0].name, "base");
        EXPECT_EQ(plan[0].profiles[0].name, "small");
        EXPECT_EQ(plan[0].profiles[0].summary_regex, "^small");
        EXPECT_EQ(plan[0].profiles[0].commands[0], "cp -a input/. .");
        EXPECT_EQ(plan[0].profiles[0].commands[1], "prepare && module list");
        EXPECT_EQ(plan[0].profiles[0].commands[2],
                  "OMP_NUM_THREADS=3 srun --ntasks=8 --ntasks-per-node=4 "
                  "--cpus-per-task=3 --cpu-bind=cores app --small > kez.out 2> kez.err");
        EXPECT_EQ(plan[0].profiles[0].commands[3], "cleanup");

        EXPECT_EQ(plan[0].profiles[1].name, "copy-small");
        EXPECT_EQ(plan[0].profiles[1].commands, plan[0].profiles[0].commands);
        EXPECT_EQ(plan[1].name, "clone");
        ASSERT_EQ(plan[1].profiles.size(), 2U);
        EXPECT_EQ(plan[1].profiles[0].commands, plan[0].profiles[0].commands);
    }

    /**
     * @brief Verifies that wrap_factory_target produces a correct Slurm
     *        sbatch wrapper command.
     *
     * The test calls wrap_factory_target with MPI configuration (mpirun
     * with --bind-to core) inside a Slurm batch environment. It checks that
     * the resulting command string is a correctly-formed sbatch invocation
     * that embeds:
     *   - The job name derived from the current working directory.
     *   - Node, task, CPU, and GPU resource allocations (2 nodes, 3 tasks
     *     per node, 4 CPUs per task, 1 GPU per task).
     *   - A custom partition (--partition=batch).
     *   - Output and error redirection (kez.out / kez.err).
     *   - The OMP_NUM_THREADS environment variable set to 8.
     *   - The inner MPI launch command wrapped via --wrap.
     *
     * Edge cases covered:
     *   - Non-trivial MPI launcher flags (--bind-to core).
     *   - Explicit GPU-per-task specification (gpus_per_task = 1).
     *   - The generated --job-name uses the shell expression
     *     "${PWD##/}" (the current directory's basename) rather than a
     *     static name, ensuring uniqueness per directory.
     */
    TEST(FactoryParser, WrapsSlurmAndMpiProfiles) {
        EXPECT_EQ(wrap_factory_target("./bench", "mpirun", "--bind-to core", "slurm",
                                      "--partition=batch", "2", "3", "4", "8", "1"),
                  "sbatch --job-name=${PWD##*/} --nodes=2 --ntasks-per-node=3 "
                  "--cpus-per-task=4 --gpus-per-task=1 --partition=batch --output=kez.out "
                  "--error=kez.err --wrap=\"OMP_NUM_THREADS=8 mpirun --bind-to core ./bench\"");
    }
}  // namespace
