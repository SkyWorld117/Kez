#pragma once

#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <string>
#include <uconf_parser/user_config_parser.hpp>
#include <vector>

/** @brief Construct the fully quoted shell command for an installation plan. */
std::string install_executor_command(const std::filesystem::path& executor,
                                     const std::filesystem::path& prefix,
                                     const std::filesystem::path& plan_path, bool force,
                                     bool with_slurm, const std::string& slurm_job);

/** @brief Selected rebuild closure and filtered executable plan. */
struct RebuildPlanSelection {
    bool target_found = false;
    std::vector<std::string> packages;
    BashCommandPlan plan;
};

/** @brief Select a rebuild target and all of its dependents from a plan. */
RebuildPlanSelection select_rebuild_plan(const BashCommandPlan& plan, const std::string& target);

/** @brief Return a new installed-state sequence with one package removed. */
YAML::Node state_without_package(const YAML::Node& state, const std::string& package);
