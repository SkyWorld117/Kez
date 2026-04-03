#pragma once

#include <yaml-cpp/yaml.h>

#include <colors/colored_io.hpp>
#include <string>

std::string wrap_with_resource_manager(
    const std::string& target, const std::string& launcher, const std::string& launcher_opts,
    const std::string& scheduler, const std::string& scheduler_opts, const std::string& num_nodes,
    const std::string& num_procs_per_node, const std::string& cores_per_proc,
    const std::string& omp_num_threads, const std::string& gpus_per_proc);