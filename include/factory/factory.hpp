#pragma once

#include <yaml-cpp/yaml.h>

#include <string>
#include <vector>

/**
 * @brief A single runtime profile within a buildspace.
 *
 * Each profile defines a named collection of bash commands that are executed
 * sequentially to run a benchmarking or profiling workload inside a factory
 * buildspace. The commands typically copy inputs, execute a launcher-wrapped
 * target, and run post-processing. An optional regex extracts summary metrics
 * from the resulting output files.
 */
struct FactoryProfile {
    /** @brief Unique name identifying this profile within its buildspace. */
    std::string name;

    /**
     * @brief Ordered list of bash commands to execute.
     *
     * The sequence is built by the parser as:
     *   1. Input copy command (cp -a <inputs>/. .)  -- if inputs are defined
     *   2. Prerun script                             -- if defined
     *   3. Launcher/scheduler-wrapped target command  -- always present
     *   4. Postrun script                             -- if defined
     */
    std::vector<std::string> commands;

    /**
     * @brief Regular expression used to extract summary lines from output logs.
     *
     * After the profile finishes, `kez factory summarize` greps the output
     * files (kez.out / kez.err) for lines matching this regex. An empty
     * string means no summarization is performed for this profile.
     */
    std::string summary_regex;
};

/**
 * @brief A named group of profiles, representing one build variant in a factory.
 *
 * A buildspace corresponds to a single set of installed packages (a build
 * variant) inside the factory. It contains one or more profiles that each
 * define how that build variant is exercised (input data, launch parameters,
 * post-processing).
 */
struct FactoryBuildspace {
    /** @brief Unique name identifying this buildspace. */
    std::string name;

    /** @brief Profiles belonging to this buildspace. */
    std::vector<FactoryProfile> profiles;
};

/**
 * @brief The top-level plan: an ordered list of buildspaces parsed from a
 *        factory configuration YAML.
 *
 * Each entry is a @ref FactoryBuildspace containing its own set of profiles.
 * The plan preserves the order in which buildspaces and profiles appear in
 * the configuration file.
 */
using FactoryPlan = std::vector<FactoryBuildspace>;

/**
 * @brief Parse a factory configuration YAML node into a @ref FactoryPlan.
 *
 * Expects the YAML to contain a top-level `factory:` map (or to be the map
 * itself) with a `buildspace` sequence. Each buildspace entry must have a
 * `name` and a `profiles` sequence. Profiles support multi-level inheritance
 * (factory -> buildspace -> profile) and sibling references for reusing
 * configuration. The parser resolves inheritance chains, detects cycles, and
 * calls @ref wrap_factory_target for each profile's launch command.
 *
 * @param config  The YAML node to parse. If it is a map containing a
 *                `factory` key, that key's value is used as the configuration
 *                root; otherwise the node itself is treated as the root.
 *
 * @return A @ref FactoryPlan containing the parsed buildspaces and their
 *         profiles, with all inheritance resolved and commands assembled.
 *
 * @warning The function terminates the program with an error message if the
 *          YAML structure is invalid (missing keys, wrong types, unresolved
 *          siblings, or cycles).
 *
 * @see wrap_factory_target
 * @see FactoryBuildspace
 * @see FactoryProfile
 */
FactoryPlan parse_factory_config(const YAML::Node& config);

/**
 * @brief Wrap a target command with launcher and/or scheduler wrappers,
 *        producing a single bash command string.
 *
 * Builds a command that runs @p target under the specified launcher (none,
 * mpirun, or srun) and/or scheduler (none or slurm). Resource counts are
 * validated as positive integers (except gpus_per_proc, which may be zero).
 * The function detects multiplication overflow when computing total
 * processes (num_nodes * num_procs_per_node).
 *
 * Scheduling modes:
 *   - **scheduler == "none"**: The command is run directly (or through the
 *     launcher) in the foreground. Output is redirected to `kez.out` and
 *     `kez.err`.
 *   - **scheduler == "slurm"**: The command is wrapped in an `sbatch`
 *     submission script. Slurm flags (--nodes, --ntasks-per-node,
 *     --cpus-per-task, --gpus-per-task) are set from the resource parameters,
 *     and scheduler_opts are appended. Output goes to `kez.out`/`kez.err` via
 *     sbatch's --output/--error. The launcher, if any, runs inside the
 *     allocation.
 *
 * Launcher modes (inside the scheduler or directly):
 *   - **"none"**: No launcher wrapper; @p target runs as-is.
 *   - **"mpirun"**: Wraps with `mpirun --prefix ... -np <total_procs>
 *     --map-by ppr:<num_procs_per_node>:node --map-by ppr:1:PE=<cores_per_proc>`.
 *     OMP_NUM_THREADS is set in the environment.
 *   - **"srun"**: Wraps with `srun --ntasks=<total_procs>
 *     --ntasks-per-node=<num_procs_per_node> --cpus-per-task=<cores_per_proc>`,
 *     optionally adding --gpus-per-task when gpus_per_proc != "0".
 *     OMP_NUM_THREADS is set in the environment.
 *
 * @param target              The executable or command to wrap.
 * @param launcher            Launcher type: "none", "mpirun", or "srun".
 * @param launcher_opts       Extra flags passed to the launcher
 *                            (e.g. "--cpu-bind=cores").
 * @param scheduler           Scheduler type: "none" or "slurm".
 * @param scheduler_opts      Extra flags passed to the scheduler
 *                            (e.g. "--partition=batch").
 * @param num_nodes           Number of compute nodes (positive integer).
 * @param num_procs_per_node  Number of MPI processes per node
 *                            (positive integer).
 * @param cores_per_proc      Number of cores allocated per process
 *                            (positive integer).
 * @param omp_num_threads     Value for OMP_NUM_THREADS (positive integer).
 *                            Defaults to cores_per_proc when inherited.
 * @param gpus_per_proc       Number of GPUs per process
 *                            (non-negative integer, default "0").
 *
 * @return A complete bash command string that runs @p target under the
 *         requested launcher and scheduler, with output redirected to
 *         kez.out/kez.err.
 *
 * @warning The function terminates the program (with an error message) if:
 *          - Any resource field is not a valid non-negative integer
 *            (or positive integer where required).
 *          - num_nodes * num_procs_per_node overflows.
 *          - An unsupported launcher or scheduler type is provided.
 *
 * @see parse_factory_config
 */
std::string wrap_factory_target(const std::string& target, const std::string& launcher,
                                const std::string& launcher_opts, const std::string& scheduler,
                                const std::string& scheduler_opts, const std::string& num_nodes,
                                const std::string& num_procs_per_node,
                                const std::string& cores_per_proc,
                                const std::string& omp_num_threads,
                                const std::string& gpus_per_proc);
