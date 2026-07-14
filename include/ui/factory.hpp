#pragma once

#include <factory/factory.hpp>
#include <filesystem>
#include <regex>
#include <string>
#include <vector>

/** @brief Render a complete factory profile runner script without filesystem access. */
std::string render_factory_profile_script(const std::filesystem::path& space,
                                          const std::filesystem::path& buildspace,
                                          const FactoryProfile& profile);

/** @brief Return factory output lines matching @p pattern in their original order. */
std::vector<std::string> matching_factory_summary_lines(const std::string& contents,
                                                        const std::regex& pattern);
