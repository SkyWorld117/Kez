#include <algorithm>
#include <dependency_resolver/toposort.hpp>
#include <functional>
#include <unordered_map>
#include <utils/colored_io.hpp>

namespace {

    /**
     * @brief DFS visit states used during topological-sort traversal.
     *
     * - `Visiting`: the node is on the current recursion stack (a back edge to
     *   this node would indicate a cycle).
     * - `Visited`:  the node and all its transitive dependencies have been
     *   fully explored and the node has been placed into the sorted output.
     */
    enum class VisitState {
        Visiting,
        Visited,
    };

    /**
     * @brief Build a human-readable description of a dependency cycle.
     *
     * Given the current DFS recursion stack and the package name that was
     * encountered a second time (the back-edge target), this function extracts
     * the cycle portion and formats it as a chain, e.g.
     * `"A -> B -> C -> A"`.
     *
     * @param stack     The DFS recursion stack up to (and including) the node
     *                  whose outgoing edge triggered the cycle.  The repeated
     *                  node appears somewhere in this stack.
     * @param repeated  The package name that was reached again, closing the
     *                  cycle (the back-edge target).
     *
     * @return A string of the form `"X -> Y -> ... -> X"` showing the
     *         circular dependency chain.
     *
     * @note  If `repeated` is not found in `stack` (which should never happen
     *        in correct usage), the returned string degenerates to
     *        `" -> repeated"`.
     */
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
    // ------------------------------------------------------------------
    // 1. Collect every package that appears anywhere in the graph.
    //    Both keys (dependents) and dependency-list entries are gathered
    //    so that isolated nodes (keys with empty dependency lists) are
    //    also included.
    // ------------------------------------------------------------------
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

    // ------------------------------------------------------------------
    // 2. Recursive DFS visitor.
    //
    //    Three-colour DFS (unvisited / visiting / visited):
    //      - First encounter                    -> mark Visiting, recurse into deps.
    //      - Back-edge to a Visiting node       -> cycle detected, abort.
    //      - Re-visit of a fully Visited node   -> no-op (already ordered).
    //
    //    After all transitive dependencies have been visited the node is
    //    appended to `sorted` (post-order), producing a reverse-topological
    //    order that is reversed back to a valid topo-order by the call site.
    //    (This implementation pushes at Visited time, yielding the correct
    //    dependency-first ordering directly.)
    // ------------------------------------------------------------------
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

    // ------------------------------------------------------------------
    // 3. Visit every package, ensuring that disconnected components are
    //    also covered.
    // ------------------------------------------------------------------
    for (const std::string& package : packages) {
        visit(package);
    }
    return sorted;
}
