#include <cerrno>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <string>
#include <ui/bash_completion.hpp>
#include <vector>

/**
 * @brief Entry point for the bash completion helper binary.
 *
 * Parses a zero-based word index from argv[1] and a sequence of command-line
 * words from argv[2..], then queries completion_suggestions() for candidate
 * completions and prints each on its own line to stdout.
 *
 * This binary is invoked by the kez completion hook when the user presses Tab
 * and is never called directly by the user.  It communicates completions
 * exclusively via stdout; any parse failure results in silent empty output
 * (exit 0 with no lines printed), which tells the shell that no completions
 * are available.
 *
 * @param argc Argument count (must be >= 3 for meaningful work).
 * @param argv Argument vector:
 *   - argv[1] : zero-based index of the word being completed within the
 *               full command line (must parse as a non-negative integer <=
 *               INT_MAX).
 *   - argv[2..]: the individual words of the command line so far.
 *
 * @return 0 on success.  Also returns 0 (silently) on any input error so that
 *         the shell never sees a non-zero exit from a completion helper.
 *
 * @note This function does NOT call ERROR(), exit(), or any other termination
 *       routine.  All pathological inputs (missing arguments, invalid integer,
 *       out-of-range index, overflow) are handled gracefully by returning 0
 *       with no output.
 *
 * @warning The parsed index is validated with strtol() followed by five
 *          checks (errno, no-progress, trailing chars, negative, > INT_MAX).
 *          Overflow of std::size_t during the reserve() call is not possible
 *          because argc is bounded by the shell's argument limit and the
 *          parsed index was already clamped to INT_MAX.
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
