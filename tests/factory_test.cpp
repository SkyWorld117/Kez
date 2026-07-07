#include <gtest/gtest.h>
#include <yaml-cpp/yaml.h>

#include <factory/factory.hpp>
#include <string>
#include <vector>

namespace {
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

    TEST(FactoryParser, WrapsSlurmAndMpiProfiles) {
        EXPECT_EQ(wrap_factory_target("./bench", "mpirun", "--bind-to core", "slurm",
                                      "--partition=batch", "2", "3", "4", "8", "1"),
                  "sbatch --job-name=${PWD##*/} --nodes=2 --ntasks-per-node=3 "
                  "--cpus-per-task=4 --gpus-per-task=1 --partition=batch --output=kez.out "
                  "--error=kez.err --wrap=\"OMP_NUM_THREADS=8 mpirun --bind-to core ./bench\"");
    }
}  // namespace
