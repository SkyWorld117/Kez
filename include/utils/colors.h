#pragma once

struct Color {
    enum Default : int { RED = 31, GREEN = 32, YELLOW = 33, BLUE = 34, MAGENTA = 35, CYAN = 36 };

    enum Modifier : int { BOLD = 1, HIGHLIGHT = 2, UNDERLINE = 4, BLINK = 5 };

    enum SpecialColor : int {
        INFO    = Color::BLUE,
        DEBUG   = Color::CYAN,
        WARNING = Color::YELLOW,
        ERROR   = Color::RED,
        SUCCESS = Color::GREEN
    };
};
