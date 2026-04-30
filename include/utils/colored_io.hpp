#pragma once

#include <utils/colors.h>

#define INFO(message)    print_info(message)
#define WARNING(message) print_warning(message)
#define ERROR(message)   print_error(message)
#define SUCCESS(message) print_success(message)
#ifdef DDEBUG
#define DEBUG(message) print_debug(message)
#else
#define DEBUG(message) \
    do {               \
    } while (0)
#endif

#include <unistd.h>  // for isatty and STDOUT_FILENO

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

// Doing some proper fucking c++, witness the magic
// This will wrap a text into the corresponding bash color codings, but the thing is that it does the
// entire string creation during compile time (this is somewhat dependent on the c++ version you are using).
// Usage: color<Comma separated list of colors and modifiers>(text to wrap);
// E.g. std::cout << color<Colors::RED, Color::BOLD, Color::BLINK>("Hello World");
// will print red bold flashing text.
template <int Last, int... Ts> inline std::string color(const std::string& text) {
    if (isatty(STDOUT_FILENO)) {
        return "\033[" + ((std::to_string(Ts) + ";") + ... + std::string("")) +
               std::to_string(Last) + "m" + text + "\033[0m";
    } else {
        return text;
    }
}

inline void print_debug(const std::string& message) {
    std::cout << color < Color::DEBUG("[D]: " + message) << std::endl;
}

inline void print_info(const std::string& message) {
    std::cout << color<Color::INFO>("[I]: " + message) << std::endl;
}

inline void print_warning(const std::string& message) {
    std::cout << color<Color::WARNING>("[W]: " + message) << std::endl;
}

inline void print_error(const std::string& message) {
    std::cerr << color<Color::ERROR>("[E]: " + message) << std::endl;
}

inline void print_success(const std::string& message) {
    std::cout << color<Color::SUCCESS>("[S]: " + message) << std::endl;
}

inline void print_text(const std::string& message, int max_width = 0, int indent = -1,
                       bool indent_first_line = true, int start_offset = 0) {
    assert(!max_width || indent < max_width);

    auto pi = [=] {
        for (int i = 0; i < indent; ++i) {
            std::cout << " ";
        }
    };
    if (!max_width) {
        if (indent_first_line) {
            pi();
        }
        std::cout << message;
    } else {
        if (indent_first_line) {
            pi();
        }
        std::stringstream ss;
        ss << message;
        int cur_length = indent_first_line * indent + start_offset;
        while (!ss.eof()) {
            std::string current;
            ss >> current;
            if (cur_length + current.size() > max_width) {
                if (indent + current.size() > max_width) {
                    int remaining = current.size();
                    int start     = 0;
                    int fit       = max_width - cur_length;
                    while (remaining > fit) {
                        std::cout << current.substr(start, fit) << "\n";
                        pi();
                        remaining -= fit;
                        start += fit;
                        fit = max_width - indent;
                    }
                    std::cout << current.substr(start, remaining) << " ";
                    cur_length = std::min((size_t) max_width,
                                          indent + current.size() +
                                              1);  // Plus one, because we have a space at the end
                } else {
                    std::cout << "\n";
                    pi();
                    std::cout << current + " ";
                    cur_length = std::min((size_t) max_width,
                                          indent + current.size() +
                                              1);  // Plus one, because we have a space at the end
                }
            } else {
                std::cout << current + " ";
                cur_length += current.size() + 1;  // Plus one, because we have a space at the end
            }
        }
    }
    std::cout << "\n";
}

inline void print_two_columns(const std::string& message1, const std::string& message2, int tab,
                              int max_width = 0) {
    std::cout << message1;
    int offset = message1.size();
    if (message1.size() + 5 > tab) {
        std::cout << "\n";
        print_text(message2, max_width, tab, true, 0);
    } else {
        for (int i = offset; i < tab; ++i) {
            std::cout << " ";
        }
        print_text(message2, max_width, tab, false, tab);
    }
}
