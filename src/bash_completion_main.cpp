#include <cerrno>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <string>
#include <ui/bash_completion.hpp>
#include <vector>

int main(int argc, char* argv[]) {
    if (argc < 3) {
        return 0;
    }

    errno             = 0;
    char* end         = nullptr;
    const long parsed = std::strtol(argv[1], &end, 10);
    if (errno != 0 || end == argv[1] || *end != '\0' || parsed < 0 || parsed > INT_MAX) {
        return 0;
    }

    std::vector<std::string> words;
    words.reserve(static_cast<std::size_t>(argc - 2));
    for (int index = 2; index < argc; ++index) {
        words.emplace_back(argv[index]);
    }
    for (const std::string& suggestion : completion_suggestions(static_cast<int>(parsed), words)) {
        std::cout << suggestion << '\n';
    }
    return 0;
}
