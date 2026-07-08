#include <yaml-cpp/yaml.h>

#include <cstdlib>
#include <filesystem>
#include <parser/user_config_parser.hpp>
#include <queue>
#include <rebuild/rebuild.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utils/colored_io.hpp>
#include <utils/string_utils.hpp>
#include <utils/yaml_utils.hpp>
#include <vector>

std::vector<std::string> load_installed_packages(const std::filesystem::path& env_prefix) {
    const std::filesystem::path state_file = env_prefix / "state.yaml";
    if (!std::filesystem::is_regular_file(state_file)) {
        return {};
    }

    YAML::Node document;
    try {
        document = YAML::LoadFile(state_file.string());
    } catch (const YAML::Exception& err) {
        ERROR("Failed to parse state file: " + state_file.string() + "\n" + err.what());
        exit(EXIT_FAILURE);
    }

    std::vector<std::string> packages;
    if (!yaml_has(document, "state")) {
        return packages;
    }
    const YAML::Node state = document["state"];
    if (state.IsNull()) {
        // `state:` with no entries (a freshly initialized environment) parses as null.
        return packages;
    }
    if (!state.IsSequence()) {
        ERROR("state.yaml 'state' entry must be a sequence: " + state_file.string());
        exit(EXIT_FAILURE);
    }
    for (const YAML::Node& entry : state) {
        if (!entry.IsScalar()) {
            ERROR("state.yaml contains a non-scalar package entry: " + state_file.string());
            exit(EXIT_FAILURE);
        }
        packages.push_back(entry.as<std::string>());
    }
    return packages;
}

std::unordered_map<std::string, std::vector<std::string>> build_dependents_map(
    const BashCommandPlan& plan) {
    std::unordered_map<std::string, std::vector<std::string>> dependents;
    for (const PackageCommands& package : plan) {
        for (const std::string& dependency : package.dependencies) {
            append_unique(dependents[dependency], package.package);
        }
    }
    return dependents;
}

std::vector<std::string> compute_rebuild_set(const BashCommandPlan& plan,
                                             const std::string& target) {
    const std::unordered_map<std::string, std::vector<std::string>> dependents =
        build_dependents_map(plan);

    std::unordered_set<std::string> visited;
    visited.insert(target);
    std::queue<std::string> frontier;
    frontier.push(target);
    while (!frontier.empty()) {
        const std::string current = frontier.front();
        frontier.pop();
        const auto found = dependents.find(current);
        if (found == dependents.end()) {
            continue;
        }
        for (const std::string& dependent : found->second) {
            if (visited.insert(dependent).second) {
                frontier.push(dependent);
            }
        }
    }

    // Emit in plan order so dependents precede the packages they depend on.
    std::vector<std::string> rebuild_set;
    for (const PackageCommands& package : plan) {
        if (visited.find(package.package) != visited.end()) {
            rebuild_set.push_back(package.package);
        }
    }
    return rebuild_set;
}

BashCommandPlan filter_plan(const BashCommandPlan& plan, const std::vector<std::string>& keep) {
    const std::unordered_set<std::string> keep_set(keep.begin(), keep.end());
    BashCommandPlan filtered;
    filtered.reserve(keep.size());
    for (const PackageCommands& package : plan) {
        if (keep_set.find(package.package) != keep_set.end()) {
            filtered.push_back(package);
        }
    }
    return filtered;
}
