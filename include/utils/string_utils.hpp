#pragma once

#include <string>
#include <type_traits>
#include <vector>

#include "dump.hpp"

std::vector<std::string> split(const std::string& str, char delimiter);

std::size_t get_length_without_color(const std::string& str);

template <typename T> std::string dump(const T& value) {
    return dump_struct < T, is_iterable<T>::value || std::is_array<T>::value > {}(value);
}
