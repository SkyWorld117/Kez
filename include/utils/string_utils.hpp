#pragma once

#include <string>
#include <vector>

std::vector<std::string> split(const std::string& str, char delimiter);

/*
std::size_t get_length_without_color(const std::string& str) {
    char delim = '\033';

    std::size_t size = 0;
    for (std::size_t i = 0; i < str.size(); ++i) {
        if (str[i] == delim) {
            for (; i < str.size(); ++i) {
                if (str[i] == 'm') {
                    break;
                }
            }
        } else {
            size += 1;
        }
    }

    return size;
}
    */