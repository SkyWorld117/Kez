#pragma once

#include <string>
#include <type_traits>
#include <vector>

#include <utils/dump.hpp>

std::vector<std::string> split(const std::string& str, char delimiter);
std::vector<std::string> split(const std::string& str, const std::vector<char>& delimiters);
std::vector<std::string> split_keep_delimiters(const std::string& str,
                                               const std::vector<char>& delimiters);

bool is_alphabetic(const std::string& str);
bool is_alphabetic(char c);
bool is_numeric(const std::string& str);
bool is_numeric(char c);

std::size_t get_length_without_color(const std::string& str);

template <typename T> std::string dump(const T& value) {
    return dump_struct < T, is_iterable<T>::value || std::is_array<T>::value > {}(value);
}
