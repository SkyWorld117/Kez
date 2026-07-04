#include <algorithm>
#include <dependency_resolver/toposort.hpp>
#include <functional>
#include <unordered_map>
#include <utils/colored_io.hpp>

namespace {

    enum class VisitState {
        Visiting,
        Visited,
    };

    std::string describe_cycle(const std::vector<std::string>& stack, const std::string& repeated) {
        const auto start = std::find(stack.begin(), stack.end(), repeated);
        std::string description;
        for (auto current = start; current != stack.end(); ++current) {
            if (!description.empty()) {
                description += " -> ";
            }
            description += *current;
        }
        description += " -> " + repeated;
        return description;
    }

}  // namespace

std::vector<std::string> topological_sort(const DependencyGraph& adjacency_list) {
    std::vector<std::string> packages;
    packages.reserve(adjacency_list.size());
    for (const auto& [package, dependencies] : adjacency_list) {
        packages.push_back(package);
        packages.insert(packages.end(), dependencies.begin(), dependencies.end());
    }
    std::sort(packages.begin(), packages.end());
    packages.erase(std::unique(packages.begin(), packages.end()), packages.end());

    std::vector<std::string> sorted;
    std::vector<std::string> stack;
    std::unordered_map<std::string, VisitState> states;
    std::function<void(const std::string&)> visit = [&](const std::string& package) {
        const auto state = states.find(package);
        if (state != states.end()) {
            if (state->second == VisitState::Visiting) {
                ERROR("Dependency cycle detected: " + describe_cycle(stack, package));
                exit(EXIT_FAILURE);
            }
            return;
        }

        states.emplace(package, VisitState::Visiting);
        stack.push_back(package);
        const auto dependencies = adjacency_list.find(package);
        if (dependencies != adjacency_list.end()) {
            for (const std::string& dependency : dependencies->second) {
                visit(dependency);
            }
        }
        stack.pop_back();
        states[package] = VisitState::Visited;
        sorted.push_back(package);
    };

    for (const std::string& package : packages) {
        visit(package);
    }
    return sorted;
}
