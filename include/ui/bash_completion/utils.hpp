#pragma once

#include <filesystem>
#include <global_config.hpp>
#include <string>
#include <utils/bash_utils.hpp>
#include <vector>

inline bool exists_in(const std::vector<std::string>& comp_words, const std::string& option) {
    return std::find(comp_words.begin(), comp_words.end(), option) != comp_words.end();
}

inline std::vector<std::string> get_database_suggestions() {
    std::vector<std::string> packages;
    std::string db_path = get_env_var("FROMAGER_DB");

    if (std::filesystem::exists(db_path) && std::filesystem::is_directory(db_path)) {
        for (const auto& entry : std::filesystem::directory_iterator(db_path)) {
            if (entry.is_regular_file() && entry.path().extension() == ".yaml") {
                packages.push_back(entry.path().stem().string());
            }
        }
    }
    return packages;
}

inline std::vector<std::string> get_cellars_suggestions() {
    std::vector<std::string> cellars;
    std::filesystem::path cellars_path = global_config::get_path("cellars");
    if (std::filesystem::exists(cellars_path) && std::filesystem::is_directory(cellars_path)) {
        for (const auto& entry : std::filesystem::directory_iterator(cellars_path)) {
            if (entry.is_directory() && entry.path().filename() != "system" &&
                entry.path().filename() != "compilers" && entry.path().filename() != "mpis" &&
                entry.path().filename() != "vendors" && entry.path().filename() != "utilities") {
                cellars.push_back(entry.path().filename().string());
            }
        }
    }
    return cellars;
}

inline std::vector<std::string> get_filesystem_suggestions(
    const int comp_cword                       = 0,
    const std::vector<std::string>& comp_words = std::vector<std::string>()) {
    std::vector<std::string> suggestions;
    std::string current_word = "";
    if (comp_cword >= 0 && comp_cword < static_cast<int>(comp_words.size())) {
        current_word = comp_words[comp_cword];
    }

    std::filesystem::path dir;
    std::string prefix;

    if (current_word.empty()) {
        dir = std::filesystem::current_path();
    } else {
        std::filesystem::path p(current_word);
        if (current_word.back() == '/') {
            dir    = p;
            prefix = current_word;
        } else {
            dir = p.parent_path();
            if (dir.empty()) {
                dir = std::filesystem::current_path();
            } else {
                prefix = dir.string() + "/";
            }
        }
    }

    std::error_code ec;
    if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec)) {
        return suggestions;
    }

    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        std::string filename   = entry.path().filename().string();
        std::string suggestion = prefix + filename;
        suggestions.push_back(suggestion);
    }
    return suggestions;
}

inline std::string get_version_from_cmdline_config(const std::string& target,
                                                   const std::vector<std::string>& config_options,
                                                   bool& version_set) {
    for (const std::string& option : config_options) {
        if (option.rfind(target + ".version=", 0) == 0) {
            version_set = true;
            return option.substr((target + ".version=").length());
        }
    }
    return "";
}

inline std::string get_compiler_spec_from_cmdline_config(
    const std::string& pkg_name, const std::vector<std::string>& config_options,
    bool& compiler_spec_set) {
    for (const std::string& option : config_options) {
        if (option.rfind(pkg_name + ".compiler=", 0) == 0) {
            std::string compiler_spec = option.substr((pkg_name + ".compiler=").length());
            compiler_spec_set         = true;
            return compiler_spec;
        }
    }
    return "";
}