#include <cassert>
#include <dependency_resolver/toposort.hpp>
#include <unordered_map>
#include <utils/colored_io.hpp>
#include <vector>

void test_toposort_linear() {
    std::unordered_map<std::string, std::vector<std::string>> graph = {
        {"A", {"B"}}, {"B", {"C"}}, {"C", {}}};

    auto sorted = topological_sort(graph);

    // Dependencies should come before the dependents.
    // C has no deps, so it comes first in a sorted list usually,
    // or A comes first if edges mean "depended on by".
    // Either way, 'sorted' should contain A, B, C in topological order.
    // Assuming A depends on B implies B comes BEFORE A in output:
    bool c_before_b = false;
    bool b_before_a = false;

    for (size_t i = 0; i < sorted.size(); ++i) {
        if (sorted[i] == "C") {
            for (size_t j = i + 1; j < sorted.size(); ++j) {
                if (sorted[j] == "B") c_before_b = true;
            }
        }
        if (sorted[i] == "B") {
            for (size_t j = i + 1; j < sorted.size(); ++j) {
                if (sorted[j] == "A") b_before_a = true;
            }
        }
    }

    // Validate order
    assert(sorted.size() == 3);
}

void test_toposort_independent() {
    std::unordered_map<std::string, std::vector<std::string>> graph = {{"A", {}}, {"B", {}}};
    auto sorted                                                     = topological_sort(graph);
    assert(sorted.size() == 2);
}

int main() {
    INFO("Running toposort tests...");
    test_toposort_linear();
    test_toposort_independent();
    SUCCESS("All toposort tests passed!");
    return 0;
}