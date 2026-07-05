#pragma once

#include <string>
#include <vector>

std::vector<std::string> completion_suggestions(int current_word_index,
                                                const std::vector<std::string>& words);
