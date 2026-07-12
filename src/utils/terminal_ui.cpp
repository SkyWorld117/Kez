#include <termios.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utils/colored_io.hpp>
#include <utils/string_utils.hpp>
#include <utils/terminal_ui.hpp>

namespace {
    termios active_terminal_settings {};
    bool raw_input_active = false;

    bool is_terminal() { return isatty(STDIN_FILENO) && isatty(STDOUT_FILENO); }

    void restore_raw_input() {
        if (raw_input_active) {
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &active_terminal_settings);
            raw_input_active = false;
        }
    }

    [[noreturn]] void input_ended(const std::string& context) {
        restore_raw_input();
        ERROR("Input ended while " + context);
        exit(EXIT_FAILURE);
    }

    char read_terminal_key(const std::string& context) {
        char input = '\0';
        while (true) {
            const ssize_t count = read(STDIN_FILENO, &input, 1);
            if (count == 1) {
                return input;
            }
            if (count == 0) {
                input_ended(context);
            }
            if (count < 0 && errno != EINTR) {
                input_ended(context);
            }
        }
    }

    bool enable_raw_input(termios& previous) {
        if (tcgetattr(STDIN_FILENO, &previous) != 0) {
            return false;
        }
        termios raw = previous;
        raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
        raw.c_cc[VMIN]  = 1;
        raw.c_cc[VTIME] = 0;
        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) {
            return false;
        }
        active_terminal_settings     = previous;
        raw_input_active             = true;
        static const bool registered = [] {
            std::atexit(restore_raw_input);
            return true;
        }();
        static_cast<void>(registered);
        return true;
    }

    void render_selector(const std::vector<std::string>& entries, const std::vector<bool>& selected,
                         std::size_t cursor, bool redraw) {
        if (redraw) {
            std::cout << "\033[" << entries.size() << "A\r\033[J";
        }
        for (std::size_t index = 0; index < entries.size(); ++index) {
            std::cout << (index == cursor ? "> " : "  ") << (selected[index] ? "[x] " : "[ ] ")
                      << entries[index] << '\n';
        }
        std::cout.flush();
    }

    std::vector<bool> terminal_selector(const std::string& prompt,
                                        const std::vector<std::string>& entries,
                                        std::vector<bool> selected, bool mutually_exclusive) {
        if (entries.empty()) {
            return selected;
        }

        std::cout << prompt << " (Use arrow keys or j/k to navigate, space to select/deselect, "
                  << "and Enter to confirm)\n";

        termios previous {};
        if (!enable_raw_input(previous)) {
            input_ended("starting the terminal selector");
        }

        std::size_t cursor = 0;
        bool redraw        = false;
        while (true) {
            render_selector(entries, selected, cursor, redraw);
            redraw     = true;
            char input = read_terminal_key("using the terminal selector");
            if (input == '\n' || input == '\r') {
                break;
            }
            if (input == 'j') {
                cursor = (cursor + 1) % entries.size();
            } else if (input == 'k') {
                cursor = (cursor + entries.size() - 1) % entries.size();
            } else if (input == ' ') {
                if (mutually_exclusive) {
                    const bool was_selected = selected[cursor];
                    std::fill(selected.begin(), selected.end(), false);
                    selected[cursor] = !was_selected;
                } else {
                    selected[cursor] = !selected[cursor];
                }
            } else if (input == '\033') {
                const char bracket = read_terminal_key("reading an arrow key");
                const char arrow   = read_terminal_key("reading an arrow key");
                if (bracket == '[' && arrow == 'A') {
                    cursor = (cursor + entries.size() - 1) % entries.size();
                } else if (bracket == '[' && arrow == 'B') {
                    cursor = (cursor + 1) % entries.size();
                }
            }
        }
        restore_raw_input();
        return selected;
    }

    bool line_confirm(const std::string& prompt, bool default_value) {
        while (true) {
            std::cout << prompt << " (y/n) [" << (default_value ? 'y' : 'n') << "]: ";
            std::cout.flush();
            std::string input;
            if (!std::getline(std::cin, input)) {
                input_ended("answering '" + prompt + "'");
            }
            input = trim(input);
            if (input.empty()) {
                return default_value;
            }
            if (input == "y" || input == "Y") {
                return true;
            }
            if (input == "n" || input == "N") {
                return false;
            }
            WARNING("Please answer y or n.");
        }
    }
}  // namespace

std::vector<bool> terminal_select_multiple(const std::string& prompt,
                                           const std::vector<std::string>& entries,
                                           const std::vector<bool>& selected) {
    if (!selected.empty() && selected.size() != entries.size()) {
        ERROR("Terminal selector received mismatched entries and selections");
        exit(EXIT_FAILURE);
    }
    std::vector<bool> result =
        selected.empty() ? std::vector<bool>(entries.size(), false) : selected;
    if (is_terminal()) {
        return terminal_selector(prompt, entries, std::move(result), false);
    }
    std::cout << prompt << '\n';
    for (std::size_t index = 0; index < entries.size(); ++index) {
        result[index] = line_confirm("Enable " + entries[index] + "?", result[index]);
    }
    return result;
}

std::optional<std::size_t> terminal_select_one(const std::string& prompt,
                                               const std::vector<std::string>& entries) {
    if (entries.empty()) {
        return std::nullopt;
    }
    if (is_terminal()) {
        const std::vector<bool> selected =
            terminal_selector(prompt, entries, std::vector<bool>(entries.size(), false), true);
        const auto choice = std::find(selected.begin(), selected.end(), true);
        return choice == selected.end() ? std::nullopt
                                        : std::optional<std::size_t>(static_cast<std::size_t>(
                                              std::distance(selected.begin(), choice)));
    }

    std::cout << prompt << '\n';
    for (const std::string& entry : entries) {
        std::cout << "[ ] " << entry << '\n';
    }
    while (true) {
        std::cout << "Enter an implementation name (leave empty for the default): ";
        std::cout.flush();
        std::string input;
        if (!std::getline(std::cin, input)) {
            input_ended("selecting an implementation");
        }
        input = trim(input);
        if (input.empty()) {
            return std::nullopt;
        }
        const auto choice = std::find(entries.begin(), entries.end(), input);
        if (choice != entries.end()) {
            return static_cast<std::size_t>(std::distance(entries.begin(), choice));
        }
        WARNING("Please enter one of the listed implementation names.");
    }
}
