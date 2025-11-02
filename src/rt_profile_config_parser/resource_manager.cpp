#include "resource_manager.h"

std::string wrap_with_resource_manager(
    const std::string& target,
    const std::string& launcher,
    const std::string& launcher_opts,
    const std::string& scheduler,
    const std::string& scheduler_opts,
    const std::string& num_nodes,
    const std::string& num_procs_per_node,
    const std::string& cores_per_proc,
    const std::string& omp_num_threads,
    const std::string& gpus_per_proc
) {
    if (scheduler == "none") {
        // Only launcher
        std::string cmd;
        if (launcher == "none") {
            cmd = target;
        } else if (launcher == "mpirun") {
            cmd = "OMP_NUM_THREADS=" + omp_num_threads + " mpirun ";
            cmd += "--prefix $(which mpirun | xargs dirname)/../ ";
            cmd += "-x OMP_NUM_THREADS -x PATH -x LD_LIBRARY_PATH ";
            cmd += "-np " + std::to_string(std::stoi(num_nodes) * std::stoi(num_procs_per_node)) + " ";
            cmd += "--map-by ppr:" + num_procs_per_node + ":node ";
            cmd += "--map-by ppr:1:PE=" + cores_per_proc + " ";
            // mpirun does not handle GPU allocation
            cmd += launcher_opts + " ";
            cmd += target;
        } else if (launcher == "srun") {
            cmd = "OMP_NUM_THREADS=" + omp_num_threads + " srun ";
            cmd += "--ntasks=" + std::to_string(std::stoi(num_nodes) * std::stoi(num_procs_per_node)) + " ";
            cmd += "--ntasks-per-node=" + num_procs_per_node + " ";
            cmd += "--cpus-per-task=" + cores_per_proc + " ";
            if (gpus_per_proc != "0") {
                cmd += "--gpus-per-task=" + gpus_per_proc + " ";
            }
            cmd += launcher_opts + " ";
            cmd += target;
        } else {
            ERROR("Unsupported launcher: " + launcher);
            exit(EXIT_FAILURE);
        }
        // Split output into fgr.out and fgr.err
        cmd += " > fgr.out 2> fgr.err";
        return cmd;
    } else if (scheduler == "slurm") {
        std::string cmd = "sbatch ";
        cmd += "--job-name=${PWD##*/} ";
        cmd += "--nodes=" + num_nodes + " ";
        cmd += "--ntasks-per-node=" + num_procs_per_node + " ";
        cmd += "--cpus-per-task=" + cores_per_proc + " ";
        if (gpus_per_proc != "0") {
            cmd += "--gpus-per-task=" + gpus_per_proc + " ";
        }
        if (!scheduler_opts.empty()) {
            cmd += scheduler_opts + " ";
        }
        cmd += "--output=fgr.out --error=fgr.err ";

        cmd += "--wrap=\"OMP_NUM_THREADS=" + omp_num_threads + " ";

        // Launcher part
        if (launcher == "none") {
            cmd += target + "\"";
        } else if (launcher == "mpirun") {
            cmd += "mpirun " + launcher_opts + " " + target + "\"";
        } else if (launcher == "srun") {
            cmd += "srun " + launcher_opts + " " + target + "\"";
        } else {
            ERROR("Unsupported launcher: " + launcher);
            exit(EXIT_FAILURE);
        }
        return cmd;
    } else {
        ERROR("Unsupported scheduler: " + scheduler);
        exit(EXIT_FAILURE);
    }

    return "";
}