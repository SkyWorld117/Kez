#pragma once

/**
 * @file colored_io.hpp
 * @brief Colored terminal output utilities with compile-time ANSI code generation.
 *
 * Provides a compile-time color-wrapping template, convenience macros, and
 * formatted printing functions (word-wrapping, two-column layout) that
 * automatically strip ANSI codes when output is not a TTY.
 */

#include <utils/colors.h>

#include <utils/string_utils.hpp>

/**
 * @def INFO(message)
 * @brief Print an informational message prefixed with "[I]:".
 *
 * @param message The text to print.
 * @see print_info
 */
#define INFO(message) std::cout << color<Color::INFO>(std::string("[I]: ") + (message)) << std::endl

/**
 * @def WARNING(message)
 * @brief Print a warning message prefixed with "[W]:".
 *
 * @param message The text to print.
 * @see print_warning
 */
#define WARNING(message) \
    std::cout << color<Color::WARNING>(std::string("[W]: ") + (message)) << std::endl

/**
 * @def ERROR(message)
 * @brief Print an error message to stderr, prefixed with "[E]:".
 *
 * @param message The text to print.
 * @see print_error
 */
#define ERROR(message) \
    std::cerr << color<Color::ERROR>(std::string("[E]: ") + (message), STDERR_FILENO) << std::endl

/**
 * @def SUCCESS(message)
 * @brief Print a success message prefixed with "[S]:".
 *
 * @param message The text to print.
 * @see print_success
 */
#define SUCCESS(message) \
    std::cout << color<Color::SUCCESS>(std::string("[S]: ") + (message)) << std::endl

/**
 * @def DEBUG(message)
 * @brief Print a debug message prefixed with "[D]:".
 *
 * This macro is only active when the `DDEBUG` preprocessor symbol is defined.
 * In release builds it expands to a no-op, imposing zero runtime overhead.
 *
 * @param message The text to print.
 * @see print_debug
 */
#ifdef DDEBUG
#define DEBUG(message) \
    std::cout << color<Color::DEBUG>(std::string("[D]: ") + (message)) << std::endl
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

/**
 * @brief Wraps a string in ANSI escape codes at (effectively) compile time.
 *
 * The template pack encodes the ANSI parameter codes (colors, modifiers) as
 * integer template arguments. The entire escape sequence is assembled at
 * runtime, but the code values are compile-time constants, allowing the
 * compiler to inline and constant-fold the sequence construction.
 *
 * If `STDOUT_FILENO` is not a terminal (`isatty` returns false), the text
 * is returned unmodified so that redirected output contains no escape codes.
 *
 * @tparam Last The last ANSI code in the sequence (e.g. `Color::RED`).
 * @tparam Ts   Zero or more preceding ANSI codes (e.g. `Color::BOLD`).
 * @param text  The plain-text string to wrap.
 * @param fd    File descriptor to test with isatty() (default STDOUT_FILENO).
 *              Pass STDERR_FILENO when the output goes to stderr so that
 *              ANSI codes are emitted when stderr is a terminal even if
 *              stdout has been redirected.
 * @return The input string wrapped in `\033[...m ... \033[0m` when the
 *         given file descriptor is a TTY, or the unchanged input otherwise.
 *
 * @par Usage example
 * @code
 *   std::cout << color<Color::RED, Color::BOLD>("Hello") << std::endl;
 *   std::cerr << color<Color::ERROR>("Oops", STDERR_FILENO) << std::endl;
 * @endcode
 *
 * @see Color (colors.h) for the available color and modifier constants.
 */
template <int Last, int... Ts>
inline std::string color(const std::string& text, int fd = STDOUT_FILENO) {
    if (isatty(fd)) {
        return "\033[" + ((std::to_string(Ts) + ";") + ... + std::string("")) +
               std::to_string(Last) + "m" + text + "\033[0m";
    } else {
        return text;
    }
}

/**
 * @brief Print text with optional word-wrapping, indentation, and
 *        ANSI-aware column-width limits.
 *
 * Wraps text at word boundaries so that no line exceeds `max_width`
 * visible characters (ANSI escape codes are not counted toward the
 * width). Words longer than the available width are broken across
 * lines at character boundaries.
 *
 * @param message          The text to print.
 * @param max_width        Maximum visible characters per line. If 0 (the
 *                         default), no wrapping is performed and the text
 *                         is printed on a single line.
 * @param indent           Number of spaces to indent each line (including
 *                         the first line when `indent_first_line` is true).
 *                         Must be less than `max_width` when both are
 *                         non-zero (asserted). A negative value (the
 *                         default) is treated as an uninitialized sentinel
 *                         and must be replaced by the caller before use.
 * @param indent_first_line Whether to indent the first line. When false,
 *                         the first line starts at column 0 and subsequent
 *                         wrapped lines use the full `indent`.
 * @param start_offset     Horizontal offset already consumed on the first
 *                         line before printing begins (used when this
 *                         function is called mid-line from
 *                         @ref print_two_columns). Only meaningful when
 *                         `indent_first_line` is true.
 *
 * @warning The caller must ensure that `indent < max_width` when both
 *          are non-zero; this is enforced by an assertion.
 *
 * @see print_two_columns
 */
inline void print_text(const std::string& message, int max_width = 0, int indent = -1,
                       bool indent_first_line = true, int start_offset = 0) {
    assert(!max_width || indent < max_width);

    auto print_indent = [=] {
        for (int i = 0; i < indent; ++i) {
            std::cout << " ";
        }
    };
    if (!max_width) {
        if (indent_first_line) {
            print_indent();
        }
        std::cout << message;
    } else {
        if (indent_first_line) {
            print_indent();
        }
        std::stringstream ss;
        ss << message;
        int cur_length = indent_first_line * indent + start_offset;
        while (!ss.eof()) {
            std::string current;
            ss >> current;
            if (cur_length + get_length_without_color(current) > max_width) {
                if (indent + get_length_without_color(current) > max_width) {
                    int remaining = get_length_without_color(current);
                    int start     = 0;
                    int fit       = max_width - cur_length;
                    while (remaining > fit) {
                        std::cout << current.substr(start, fit) << "\n";
                        print_indent();
                        remaining -= fit;
                        start += fit;
                        fit = max_width - indent;
                    }
                    std::cout << current.substr(start, remaining) << " ";
                    cur_length = std::min(max_width,
                                          indent + get_length_without_color(current) +
                                              1);  // Plus one, because we have a space at the end
                } else {
                    std::cout << "\n";
                    print_indent();
                    std::cout << current + " ";
                    cur_length = std::min(max_width,
                                          indent + get_length_without_color(current) +
                                              1);  // Plus one, because we have a space at the end
                }
            } else {
                std::cout << current + " ";
                cur_length += get_length_without_color(current) +
                              1;  // Plus one, because we have a space at the end
            }
        }
    }
    std::cout << "\n";
}

/**
 * @brief Print a two-column layout, with the second column starting at a
 *        fixed tab stop and wrapping independently.
 *
 * If the first column (including a three-space gap) exceeds the tab stop,
 * the second column starts on the next line. The second column's text is
 * word-wrapped via @ref print_text.
 *
 * @param message1 Text for the left column.
 * @param message2 Text for the right column.
 * @param tab      Column (in characters) at which the right column starts.
 * @param max_width Maximum visible width for the right column's text; passed
 *                  through to @ref print_text. 0 means no wrapping.
 *
 * @see print_text
 */
inline void print_two_columns(const std::string& message1, const std::string& message2, int tab,
                              int max_width = 0) {
    std::cout << message1;
    int offset = get_length_without_color(message1);
    if (get_length_without_color(message1) + 3 > tab) {
        std::cout << "\n";
        print_text(message2, max_width, tab, true, 0);
    } else {
        for (int i = offset; i < tab; ++i) {
            std::cout << " ";
        }
        print_text(message2, max_width, tab, false, tab);
    }
}
