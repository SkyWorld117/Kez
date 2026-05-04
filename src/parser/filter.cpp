#include <parser/filter.hpp>
#include <sstream>
#include <unordered_set>

// Search for variable/option patterns <name>="<value>" and filter remove the duplicate terms in the value.
// Replace inplace
void filter(std::string& input) {
    size_t pos = 0;
    while ((pos = input.find('"', pos)) != std::string::npos) {
        size_t end = input.find('"', pos + 1);
        if (end == std::string::npos) {
            ERROR("Unmatched quotes in input string");
            exit(EXIT_FAILURE);
        }

        std::unordered_set<std::string> seen;
        std::string term = input.substr(pos + 1, end - pos - 1);
        std::stringstream ss(term);
        std::string subTerm, joined;

        while (ss >> subTerm) {
            if (seen.insert(subTerm).second) {
                if (!joined.empty()) joined += ' ';
                joined += subTerm;
            }
        }

        input.replace(pos + 1, end - pos - 1, joined);
        pos += joined.length() + 2;  // Move past the replaced term
    }
}
