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

    /**
     * @brief Checks whether any of the given values appear in a vector.
     *
     * Returns true as soon as the first matching element is found; short-circuits
     * on the first hit.
     *
     * @param words   The vector to search through.
     * @param values  The set of candidate strings to look for.
     *
     * @return true if at least one element of `values` exists in `words`,
     *         false if none do.
     */
    bool contains_any(const std::vector<std::string>& words,
                      const std::vector<std::string>& values) {
        return std::any_of(values.begin(), values.end(),
                           [&words](const std::string& value) { return contains(words, value); });
    }

    /**
     * @brief Safely retrieves the word at a given index from the command-line
     *        token vector.
     *
     * Returns an empty string when the index is out of bounds (negative or
     * beyond the last element).  This guards against malformed or incomplete
     * command lines during early-tab completion.
     *
     * @param words  The tokenized command line.
     * @param index  The zero-based index of the desired word.
     *
     * @return The word at `index`, or an empty string if the index is invalid.
     */
    std::string word_at(const std::vector<std::string>& words, int index) {
        if (index < 0 || static_cast<std::size_t>(index) >= words.size()) {
            return "";
        }
        return words[static_cast<std::size_t>(index)];
    }

    /**
     * @brief Scans the package database directory for available package names.
     *
     * Reads the `KEZ_DB` environment variable to locate the database directory.
     * If the variable is unset, empty, or does not point to an existing
     * directory, an empty vector is returned silently.  Each immediate
     * subdirectory inside the database root is treated as a package name.
     *
     * @return A vector of package names (directory basenames), or an empty
     *         vector if the database cannot be read.
     *
     * @note Uses `get_env_var_noerr()` so a missing `KEZ_DB` does not
     *       terminate the process — completion degrades gracefully.
     */
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

    /**
     * @brief Lists the immediate subdirectory names under a configured
     *        environment-specific path.
     *
     * Requires both `KEZ_HOME` and `KEZ_WORKDIR` to be set (non-empty).
     * Resolves the target directory by calling `configured_work_path(path_name)`
     * and scans its immediate child directories.  Returns an empty vector if
     * either environment variable is missing or the resolved path does not exist.
     *
     * This is used to suggest existing environment, compiler, MPI, or factory
     * instance names for subcommands like `env enter`, `compiler load`, or
     * `factory enter`.
     *
     * @param path_name  The sub-path relative to the work directory's
     *                   `.kez/environments/` area (e.g. "applications",
     *                   "compilers", "mpis", "factories").
     *
     * @return A vector of directory basenames found under the resolved path,
     *         or an empty vector if the path is inaccessible.
     *
     * @see configured_work_path()  Resolves the base path for a given name.
     */
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

    /**
     * @brief Generates filesystem-path suggestions for tab completion.
     *
     * Parses the current word being completed to determine the target directory:
     *   - If `current_word` is empty, the current working directory is used.
     *   - If it ends with a `/`, it is treated as a directory path directly.
     *   - Otherwise, the parent directory is used and the basename becomes a
     *     prefix filter through the shell's completion mechanism.
     *
     * Each entry in the target directory is returned with a prefix so that
     * relative paths are correctly reconstructed.  Non-existent or
     * non-directory paths produce an empty result silently.
     *
     * @param current_word  The partial word being completed (may be empty).
     *
     * @return A vector of path strings suitable as completion candidates,
     *         or an empty vector if the directory cannot be listed.
     */
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

    /**
     * @brief Appends all elements of one vector to another.
     *
     * Equivalent to `target.insert(target.end(), values.begin(), values.end())`.
     *
     * @param target  The destination vector that receives the new elements.
     * @param values  The source vector whose elements are copied.
     */
    void append(std::vector<std::string>& target, const std::vector<std::string>& values) {
        target.insert(target.end(), values.begin(), values.end());
    }

    /**
     * @brief Returns the standard help-flag options common to most subcommands.
     *
     * @return A vector containing "-h" and "--help".
     */
    std::vector<std::string> help_options() { return {"-h", "--help"}; }

    /**
     * @brief Generates completion suggestions for the `kez init` subcommand.
     *
     * Always includes the standard help flags.  Adds `--refresh` and
     * `--use-distro-compiler` only if they have not already been specified
     * on the command line, preventing redundant suggestions.
     *
     * @param words  The full tokenized command line.
     *
     * @return A vector of eligible option strings for `kez init`.
     */
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

    /**
     * @brief Generates completion suggestions for the `kez update` subcommand.
     *
     * Always includes the standard help flags.  Adds `--with-system` if it
     * has not already been used on the command line.
     *
     * @param words  The full tokenized command line.
     *
     * @return A vector of eligible option strings for `kez update`.
     */
    std::vector<std::string> update_suggestions(const std::vector<std::string>& words) {
        std::vector<std::string> result = help_options();
        if (!contains(words, "--with-system")) {
            result.push_back("--with-system");
        }
        return result;
    }

    /**
     * @brief Generates completion suggestions for the `kez env` subcommand.
     *
     * At position 2 (immediately after `env`), suggests the available actions:
     * `create`, `remove`, `list`, `enter`, `exit`, `which`, `empty`, plus
     * help flags.  At position 3 and when the action is one that takes an
     * environment name (`create`, `remove`, `enter`, `empty`), lists the
     * configured application directories as candidates.
     *
     * @param current_word_index  Zero-based index of the word being completed.
     * @param words               The full tokenized command line.
     *
     * @return A vector of completion suggestions, or an empty vector if no
     *         suggestions apply at the current position.
     */
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

    /**
     * @brief Generates completion suggestions for managed-environment subcommands
     *        (`kez compiler` and `kez mpi`).
     *
     * At position 2 suggests the available managed-environment actions (`load`,
     * `unload`, `list`, `which`, `remove`, plus help flags).  At position 3,
     * when the action is `load` or `remove`, produces help flags and the list of
     * configured directories for the given `path_name` (e.g. "compilers" or
     * "mpis") as completion candidates.
     *
     * @param current_word_index  Zero-based index of the word being completed.
     * @param words               The full tokenized command line.
     * @param path_name           The subdirectory name under the configured work
     *                            path (e.g. "compilers" for `kez compiler`,
     *                            "mpis" for `kez mpi`).
     *
     * @return A vector of completion suggestions, or an empty vector if no
     *         suggestions apply.
     *
     * @see configured_directories()  Resolves the directory listing.
     */
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

    /**
     * @brief Generates completion suggestions for the `kez install` subcommand
     *        (and for `kez utilities add` when `utility` is true).
     *
     * Handles context-sensitive completions for:
     *   - File-path arguments following `--read` / `-r`.
     *   - Environment-name arguments following `--env` / `-e`.
     *   - Package-name arguments (from the database) at the first positional
     *     argument position.
     *   - Flags that have not yet been used on the command line:
     *     `--dry-run` / `-d`, `--force` / `-f`, `--with-slurm` / `-S`,
     *     `--read` / `-r` (plus database packages), `--rebuild` / `-R`,
     *     `--config` / `-c`, and `--env` / `-e` (the last is omitted when
     *     `utility` is true since `kez utilities add` does not accept an
     *     environment override).
     *
     * @param current_word_index  Zero-based index of the word being completed.
     * @param words               The full tokenized command line.
     * @param utility             If true, the completion context is
     *                            `kez utilities add` (position-3 start) rather
     *                            than `kez install` (position-2 start), and the
     *                            `--env` / `-e` flag is excluded.
     *
     * @return A vector of completion suggestions.
     *
     * @note Single-use flags that have already appeared on the command line are
     *       suppressed to avoid suggesting them again.
     */
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

    /**
     * @brief Generates completion suggestions for the `kez utilities` subcommand.
     *
     * At position 2 suggests the available utilities actions (`add`, `empty`,
     * plus help flags).  When the action is `add`, delegates to
     * `install_suggestions()` with `utility=true` to share the install-flag
     * completion logic (which excludes the `--env` / `-e` flag).
     *
     * @param current_word_index  Zero-based index of the word being completed.
     * @param words               The full tokenized command line.
     *
     * @return A vector of completion suggestions, or an empty vector if no
     *         suggestions apply.
     *
     * @see install_suggestions()  Shared logic reused for `kez utilities add`.
     */
    std::vector<std::string> utilities_suggestions(int current_word_index,
                                                   const std::vector<std::string>& words) {
        if (current_word_index == 2) {
            return {"add", "empty", "-h", "--help"};
        }
        if (word_at(words, 2) == "add") {
            return install_suggestions(current_word_index, words, true);
        }
        return {};
    }

    /**
     * @brief Generates completion suggestions for the `kez factory` subcommand.
     *
     * At position 2 suggests factory actions (`create`, `remove`, `list`,
     * `enter`, `exit`, `which`, `build`, `run`, `summarize`, plus help flags).
     *
     * At position 3:
     *   - For `enter` or `remove`: offers help flags and the list of configured
     *     factory directories.
     *   - For `build`: offers help flags plus the single-use flags
     *     `--dry-run` / `-d`, `--force` / `-f`, and `--with-slurm` / `-S` if
     *     they have not already appeared on the command line.
     *
     * @param current_word_index  Zero-based index of the word being completed.
     * @param words               The full tokenized command line.
     *
     * @return A vector of completion suggestions, or an empty vector if no
     *         suggestions apply.
     */
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

    /**
     * @brief Generates completion suggestions for the `kez uconf` subcommand.
     *
     * Handles:
     *   - File-path suggestions following the `--save` / `-s` flag.
     *   - Package-name suggestions (from the database) at all positions.
     *   - The `--save` / `-s` flag, suggested only if it has not already been
     *     used.
     *   - Help flags at the first positional argument (position 2).
     *
     * @param current_word_index  Zero-based index of the word being completed.
     * @param words               The full tokenized command line.
     *
     * @return A vector of completion suggestions.
     */
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

    /**
     * @brief Generates completion suggestions for the `kez info` subcommand.
     *
     * At position 2, offers help flags and package-name suggestions from the
     * database.  At position 3, offers the `--raw` / `-r` flag (but only if
     * it has not already been used on the command line).
     *
     * @param current_word_index  Zero-based index of the word being completed.
     * @param words               The full tokenized command line.
     *
     * @return A vector of completion suggestions, or an empty vector if no
     *         suggestions apply.
     */
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

    /**
     * @brief Top-level dispatcher for generating completion suggestions based on
     *        the current subcommand.
     *
     * Inspects the first positional argument (index 1) to identify the active
     * subcommand and delegates to the appropriate subcommand-specific helper:
     *   - "init"       -> init_suggestions()
     *   - "update"     -> update_suggestions()
     *   - "install"    -> install_suggestions()
     *   - "utilities"  -> utilities_suggestions()
     *   - "env"        -> environment_suggestions()
     *   - "compiler"   -> managed_environment_suggestions("compilers")
     *   - "mpi"        -> managed_environment_suggestions("mpis")
     *   - "factory"    -> factory_suggestions()
     *   - "uconf"      -> uconf_suggestions()
     *   - "info"       -> info_suggestions()
     *   - "selfcheck"  -> help_options()
     *
     * At index 1 itself (no subcommand yet), the function returns the full set
     * of top-level subcommand names plus global flags (`-h`, `--help`, `-V`,
     * `--version`).
     *
     * @param current_word_index  Zero-based index of the word being completed.
     * @param words               The full tokenized command line.
     *
     * @return A vector of raw (unfiltered) completion suggestions for the
     *         identified subcommand context, or an empty vector if the
     *         subcommand is unrecognised.
     */
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

/**
 * @brief Entry point for bash-completion suggestion generation.
 *
 * Called by the completion system (typically via the `kez_completion` binary)
 * to obtain a context-sensitive list of possible completions for the current
 * command line.
 *
 * The function works in four phases:
 *   1. **Dispatch** — `command_suggestions()` inspects the tokenized command
 *      line and delegates to the subcommand-specific helper.
 *   2. **Filter** — Every suggestion that does not share the current word's
 *      prefix is removed, so the shell only sees candidates that match what
 *      the user has already typed.
 *   3. **Sort** — The remaining candidates are sorted lexicographically.
 *   4. **Deduplicate** — Consecutive duplicates (from overlapping flag sets)
 *      are removed.
 *
 * @param current_word_index  Zero-based index of the word that the cursor is
 *                            currently on within `words`.
 * @param words               The full tokenized command line as a vector of
 *                            strings (e.g. `{"kez", "install", "--"}`, or
 *                            `{"kez", "install", ""}` when a space follows
 *                            "install").
 *
 * @return A filtered, sorted, and deduplicated vector of completion strings.
 *         Returns an empty vector when no applicable completions are found.
 *
 * @note The function never terminates the process — all error conditions
 *       (missing environment variables, inaccessible directories, invalid
 *       indices) are handled gracefully by returning an empty result so that
 *       the shell's completion continues to function.
 *
 * @see command_suggestions()  The dispatch logic for all subcommands.
 */
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
