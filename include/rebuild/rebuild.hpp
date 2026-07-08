#pragma once

#include <filesystem>
#include <parser/user_config_parser.hpp>
#include <string>
#include <unordered_map>
#include <vector>

// Read the `state:` sequence of an environment's state.yaml into a list of
// installed package names. Returns an empty list if the file is absent.
std::vector<std::string> load_installed_packages(const std::filesystem::path& env_prefix);

// Invert a BashCommandPlan's per-package `.dependencies` edges into a dependents
// map: result[T] = packages that declare T as a dependency. Edges are already
// concrete (abstract->concrete resolved) and filtered to buildable packages by
// the user-config parser, so the inversion yields correct dependents.
std::unordered_map<std::string, std::vector<std::string>> build_dependents_map(
    const BashCommandPlan& plan);

// The target plus the transitive closure of its dependents. The result is in
// plan order (the order packages appear in `plan`), so dependents come before
// the packages they depend on.
std::vector<std::string> compute_rebuild_set(const BashCommandPlan& plan,
                                             const std::string& target);

// Keep only the PackageCommands whose package is in `keep`, preserving plan
// order. Dependency edges are left untouched; install.sh ignores edges pointing
// at packages absent from the plan.
BashCommandPlan filter_plan(const BashCommandPlan& plan, const std::vector<std::string>& keep);
