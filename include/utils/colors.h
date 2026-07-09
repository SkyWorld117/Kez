#pragma once

/** @brief Container for ANSI escape code constants used in terminal output.
 *
 *  This struct holds three nested enumerations that together define the
 *  colour and text-style codes consumed by the printing utilities in
 *  @ref colored_io.hpp.  It is intentionally an empty struct used only
 *  as a namespace-like grouping; no instantiation is required or
 *  expected.
 *
 *  All values are ANSI SGR (Select Graphic Rendition) parameters meant
 *  to be embedded in an escape sequence of the form `\033[Nm` where N
 *  is the numeric value.
 *
 *  @see colored_io.hpp  For the functions that apply these codes. */
struct Color {
    /** @brief Standard terminal foreground colour codes.
     *
     *  Each enumerator holds the ANSI SGR colour number for the
     *  corresponding foreground colour.  These values are passed
     *  directly into the `\033[Nm` escape sequence to set the
     *  terminal text colour. */
    enum Default : int {
        RED     = 31, /**< Red foreground. */
        GREEN   = 32, /**< Green foreground. */
        YELLOW  = 33, /**< Yellow foreground. */
        BLUE    = 34, /**< Blue foreground. */
        MAGENTA = 35, /**< Magenta foreground. */
        CYAN    = 36  /**< Cyan foreground. */
    };

    /** @brief ANSI text-modifier codes (non-colour SGR parameters).
     *
     *  These values enable or disable common text-rendering
     *  attributes such as bold intensity, underline, or blink.
     *  They can be combined with a colour code by emitting the
     *  modifier first, separated by a semicolon inside the escape
     *  sequence (`\033[1;31m` for bold red). */
    enum Modifier : int {
        BOLD      = 1, /**< Bold or increased intensity. */
        HIGHLIGHT = 2, /**< Dim or decreased intensity (inverse of BOLD on some terminals). */
        UNDERLINE = 4, /**< Single underline. */
        BLINK     = 5  /**< Slow blink (may be ignored by some terminals). */
    };

    /** @brief Semantic colour aliases mapped to the default foreground colours.
     *
     *  These constants give a meaningful name to a colour based on
     *  the conventional use case, allowing callers to write
     *  `Color::ERROR` instead of `Color::RED` and to easily
     *  re-theme the application by updating the mapping in one
     *  place. */
    enum SpecialColor : int {
        INFO    = Color::BLUE,   /**< Informational messages: blue. */
        DEBUG   = Color::CYAN,   /**< Debug / verbose messages: cyan. */
        WARNING = Color::YELLOW, /**< Warning messages: yellow. */
        ERROR   = Color::RED,    /**< Error / fatal messages: red. */
        SUCCESS = Color::GREEN   /**< Success / completion messages: green. */
    };
};
