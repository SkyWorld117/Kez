#include "filter.h"

// Search for variable/option patterns <name>="<value>" and filter remove the duplicate terms in the value.
// Replace inplace
void filter(std::string& input) {
    std::vector<std::string> terms;
    size_t pos = 0;

    while ((pos = input.find('"', pos)) != std::string::npos) {
        size_t end = input.find('"', pos + 1);
        if (end == std::string::npos) {
            ERROR("Unmatched quotes in input string");
            exit(EXIT_FAILURE);
        }
        std::string term = input.substr(pos + 1, end - pos - 1);
        terms.clear();
        size_t termPos = 0;
        while ((termPos = term.find(' ')) != std::string::npos) {
            std::string subTerm = term.substr(0, termPos);
            if (std::find(terms.begin(), terms.end(), subTerm) == terms.end()) {
                terms.push_back(subTerm);
            }
            term.erase(0, termPos + 1);
        }
        // Join terms with a space
        std::string joined;
        for (size_t i = 0; i < terms.size(); ++i) {
            joined += terms[i];
            if (i != terms.size() - 1) joined += ' ';
        }
        input.replace(pos + 1, end - pos - 1, joined.empty() ? term : joined);
        pos += joined.empty() ? term.length() + 2 : joined.length() + 2; // Move past the replaced term
    }
}