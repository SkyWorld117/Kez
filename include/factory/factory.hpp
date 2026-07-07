#pragma once

#include <yaml-cpp/yaml.h>

#include <string>
#include <vector>

struct FactoryProfile {
    std::string name;
    std::vector<std::string> commands;
    std::string summary_regex;
};

struct FactoryBuildspace {
    std::string name;
    std::vector<FactoryProfile> profiles;
};

using FactoryPlan = std::vector<FactoryBuildspace>;

FactoryPlan parse_factory_config(const YAML::Node& config);

std::string wrap_factory_target(const std::string& target, const std::string& launcher,
                                const std::string& launcher_opts, const std::string& scheduler,
                                const std::string& scheduler_opts, const std::string& num_nodes,
                                const std::string& num_procs_per_node,
                                const std::string& cores_per_proc,
                                const std::string& omp_num_threads,
                                const std::string& gpus_per_proc);
