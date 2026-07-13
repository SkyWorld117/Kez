#include <algorithm>
#include <filesystem>
#include <string>
#include <ui/bash_completion.hpp>
#include <ui/ui_utils.hpp>
#include <utility>
#include <utils/bash_utils.hpp>
#include <vector>

namespace {
    /**
     * @brief Checks whether a string value is present in a vector of strings.
     *
     * Performs a linear search using std::find.
     *
     * @param words  The vector to search through.
     * @param value  The string to look for.
     *
     * @return true if `value` appears in `words`, false otherwise.
     */
    bool contains(const std::vector<std::string>& words, const std::string& value) {
        return std::find(words.begin(), words.end(), value) != words.end();
    }

    bool contains_any(const std::vector<std::string>& words,
                      const std::vector<std::string>& values) {
        return std::any_of(values.begin(), values.end(),
                           [&words](const std::string& value) { return contains(words, value); });
    }

    std::string word_at(const std::vector<std::string>& words, int index) {
        if (index < 0 || static_cast<std::size_t>(index) >= words.size()) {
            return "";
        }
        return words[static_cast<std::size_t>(index)];
    }

    std::vector<std::string> database_packages() {
        const std::filesystem::path database = get_env_var_noerr("KEZ_DB");
        std::error_code error;
        if (database.empty() || !std::filesystem::is_directory(database, error)) {
            return {};
        }

        std::vector<std::string> result;
        for (std::filesystem::directory_iterator current(database, error), end;
             !error && current != end; current.increment(error)) {
            if (current->is_directory(error)) {
                result.push_back(current->path().filename().string());
            }
        }
        return result;
    }

    std::vector<std::string> configured_directories(const std::string& path_name) {
        if (get_env_var_noerr("KEZ_HOME").empty() || get_env_var_noerr("KEZ_WORKDIR").empty()) {
            return {};
        }
        const std::filesystem::path root = configured_work_path(path_name);
        std::error_code error;
        if (!std::filesystem::is_directory(root, error)) {
            return {};
        }

        std::vector<std::string> result;
        for (std::filesystem::directory_iterator current(root, error), end;
             !error && current != end; current.increment(error)) {
            if (current->is_directory(error)) {
                result.push_back(current->path().filename().string());
            }
        }
        return result;
    }

    std::vector<std::string> filesystem_entries(const std::string& current_word) {
        std::filesystem::path directory;
        std::string prefix;
        if (current_word.empty()) {
            directory = std::filesystem::current_path();
        } else {
            const std::filesystem::path current_path(current_word);
            if (current_word.back() == '/') {
                directory = current_path;
                prefix    = current_word;
            } else {
                directory = current_path.parent_path();
                if (directory.empty()) {
                    directory = std::filesystem::current_path();
                } else {
                    prefix = directory.string() + "/";
                }
            }
        }

        std::error_code error;
        if (!std::filesystem::is_directory(directory, error)) {
            return {};
        }
        std::vector<std::string> result;
        for (std::filesystem::directory_iterator current(directory, error), end;
             !error && current != end; current.increment(error)) {
            std::string suggestion = prefix + current->path().filename().string();
            result.push_back(std::move(suggestion));
        }
        return result;
    }

    void append(std::vector<std::string>& target, const std::vector<std::string>& values) {
        target.insert(target.end(), values.begin(), values.end());
    }

    std::vector<std::string> help_options() { return {"-h", "--help"}; }

    std::vector<std::string> init_suggestions(const std::vector<std::string>& words) {
        std::vector<std::string> result = help_options();
        if (!contains(words, "--refresh")) {
            result.push_back("--refresh");
        }
        if (!contains(words, "--use-distro-compiler")) {
            result.push_back("--use-distro-compiler");
        }
        return result;
    }

    std::vector<std::string> update_suggestions(const std::vector<std::string>& words) {
        std::vector<std::string> result = help_options();
        if (!contains(words, "--with-system")) {
            result.push_back("--with-system");
        }
        return result;
    }

    std::vector<std::string> environment_suggestions(int current_word_index,
                                                     const std::vector<std::string>& words) {
        if (current_word_index == 2) {
            return {"create", "remove", "list", "enter", "exit", "which", "empty", "-h", "--help"};
        }
        const std::string action = word_at(words, 2);
        if (current_word_index == 3 &&
            (action == "create" || action == "remove" || action == "enter" || action == "empty")) {
            return configured_directories("applications");
        }
        return {};
    }

    std::vector<std::string> managed_environment_suggestions(int current_word_index,
                                                             const std::vector<std::string>& words,
                                                             const std::string& path_name) {
        if (current_word_index == 2) {
            return {"load", "unload", "list", "which", "remove", "-h", "--help"};
        }
        const std::string action = word_at(words, 2);
        if (current_word_index == 3 && (action == "load" || action == "remove")) {
            std::vector<std::string> result = help_options();
            append(result, configured_directories(path_name));
            return result;
        }
        return {};
    }

    std::vector<std::string> install_suggestions(int current_word_index,
                                                 const std::vector<std::string>& words,
                                                 bool utility) {
        const std::string previous = word_at(words, current_word_index - 1);
        const std::string current  = word_at(words, current_word_index);
        if (previous == "--read" || previous == "-r") {
            return filesystem_entries(current);
        }
        if (previous == "--env" || previous == "-e") {
            return configured_directories("applications");
        }
        if (previous == "--config" || previous == "-c") {
            return {};
        }

        std::vector<std::string> result;
        if (current_word_index == (utility ? 3 : 2)) {
            append(result, help_options());
        }
        if (!contains_any(words, {"--dry-run", "-d"})) {
            append(result, {"--dry-run", "-d"});
        }
        if (!contains_any(words, {"--force", "-f"})) {
            append(result, {"--force", "-f"});
        }
        if (!contains_any(words, {"--with-slurm", "-S"})) {
            append(result, {"--with-slurm", "-S"});
        }
        if (!contains_any(words, {"--read", "-r"})) {
            append(result, {"--read", "-r"});
            append(result, database_packages());
        }
        if (!contains_any(words, {"--rebuild", "-R"})) {
            append(result, {"--rebuild", "-R"});
        }
        if (!contains_any(words, {"--config", "-c"})) {
            append(result, {"--config", "-c"});
        }
        if (!utility && !contains_any(words, {"--env", "-e"})) {
            append(result, {"--env", "-e"});
        }
        return result;
    }

    std::vector<std::string> utilities_suggestions(int current_word_index,
                                                   const std::vector<std::string>& words) {
        if (current_word_index == 2) {
            return {"add", "reload", "empty", "-h", "--help"};
        }
        if (word_at(words, 2) == "add") {
            return install_suggestions(current_word_index, words, true);
        }
        return {};
    }

    std::vector<std::string> factory_suggestions(int current_word_index,
                                                 const std::vector<std::string>& words) {
        if (current_word_index == 2) {
            return {"create", "remove", "list",      "enter", "exit",  "which",
                    "build",  "run",    "summarize", "-h",    "--help"};
        }
        const std::string action = word_at(words, 2);
        if (current_word_index == 3 && (action == "enter" || action == "remove")) {
            std::vector<std::string> result = help_options();
            append(result, configured_directories("factories"));
            return result;
        }
        if (action == "build") {
            std::vector<std::string> result;
            if (current_word_index == 3) {
                append(result, help_options());
            }
            if (!contains_any(words, {"--dry-run", "-d"})) {
                append(result, {"--dry-run", "-d"});
            }
            if (!contains_any(words, {"--force", "-f"})) {
                append(result, {"--force", "-f"});
            }
            if (!contains_any(words, {"--with-slurm", "-S"})) {
                append(result, {"--with-slurm", "-S"});
            }
            return result;
        }
        return {};
    }

    std::vector<std::string> uconf_suggestions(int current_word_index,
                                               const std::vector<std::string>& words) {
        const std::string current  = word_at(words, current_word_index);
        const std::string previous = word_at(words, current_word_index - 1);

        if (previous == "--save" || previous == "-s") {
            return filesystem_entries(current);
        }

        std::vector<std::string> result;
        if (current_word_index == 2) {
            append(result, {"-h", "--help"});
        }
        append(result, database_packages());
        if (!contains_any(words, {"--save", "-s"})) {
            append(result, {"--save", "-s"});
        }
        return result;
    }

    std::vector<std::string> info_suggestions(int current_word_index,
                                              const std::vector<std::string>& words) {
        if (current_word_index == 2) {
            std::vector<std::string> result = help_options();
            append(result, database_packages());
            return result;
        }
        if (current_word_index == 3 && !contains_any(words, {"--raw", "-r"})) {
            return {"--raw", "-r"};
        }
        return {};
    }

    std::vector<std::string> command_suggestions(int current_word_index,
                                                 const std::vector<std::string>& words) {
        if (current_word_index == 1) {
            return {"init",      "update",   "install", "uconf",   "utilities",
                    "env",       "compiler", "mpi",     "factory", "info",
                    "selfcheck", "-h",       "--help",  "-V",      "--version"};
        }

        const std::string command = word_at(words, 1);
        if (command == "init") {
            return init_suggestions(words);
        }
        if (command == "update") {
            return update_suggestions(words);
        }
        if (command == "install") {
            return install_suggestions(current_word_index, words, false);
        }
        if (command == "utilities") {
            return utilities_suggestions(current_word_index, words);
        }
        if (command == "env") {
            return environment_suggestions(current_word_index, words);
        }
        if (command == "compiler") {
            return managed_environment_suggestions(current_word_index, words, "compilers");
        }
        if (command == "mpi") {
            return managed_environment_suggestions(current_word_index, words, "mpis");
        }
        if (command == "factory") {
            return factory_suggestions(current_word_index, words);
        }
        if (command == "uconf") {
            return uconf_suggestions(current_word_index, words);
        }
        if (command == "info") {
            return info_suggestions(current_word_index, words);
        }
        if (command == "selfcheck") {
            return help_options();
        }
        return {};
    }
}  // namespace

std::vector<std::string> completion_suggestions(int current_word_index,
                                                const std::vector<std::string>& words) {
    const std::string current            = word_at(words, current_word_index);
    std::vector<std::string> suggestions = command_suggestions(current_word_index, words);
    suggestions.erase(std::remove_if(suggestions.begin(), suggestions.end(),
                                     [&current](const std::string& value) {
                                         return value.rfind(current, 0) != 0;
                                     }),
                      suggestions.end());
    std::sort(suggestions.begin(), suggestions.end());
    suggestions.erase(std::unique(suggestions.begin(), suggestions.end()), suggestions.end());
    return suggestions;
}
