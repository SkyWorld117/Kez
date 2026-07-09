#include <cerrno>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <string>
#include <ui/bash_completion.hpp>
#include <vector>

/**
 * @brief Standalone binary that provides bash completion suggestions for kez.
 *
 * Reads the current word index (argv[1]) and the command-line words (argv[2..])
 * from the bash completion system, then prints matching suggestions to stdout,
 * one per line.  The output is consumed by completion.bash.
 *
 * If fewer than 3 arguments are provided, or if the word index is not a valid
 * non-negative integer, the program silently exits with no output.
 */
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
