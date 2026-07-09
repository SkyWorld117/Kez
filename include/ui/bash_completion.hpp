#pragma once

#include <string>
#include <vector>

/**
 * @brief Generates context-aware bash completion suggestions for the `kez` CLI.
 *
 * Inspects the full command line split into words, identifies the active
 * subcommand and the position being completed, and returns a filtered,
 * sorted, and deduplicated list of possible completions.
 *
 * Suggestions are derived from:
 *   - The set of valid subcommands (`init`, `install`, `env`, `compiler`,
 *     `mpi`, `factory`, `uconf`, `info`, `utilities`, `selfcheck`, etc.).
 *   - Subcommand-specific flags and options (e.g. `--dry-run`, `--force`,
 *     `--env`, `--read`).  Single-use options that have already appeared on
 *     the command line are **not** suggested again.
 *   - Package names discovered by scanning the directory pointed to by the
 *     `KEZ_DB` environment variable.
 *   - Configured environment, compiler, MPI, and factory directory names
 *     under the path resolved via `configured_work_path()` (requires
 *     `KEZ_HOME` and `KEZ_WORKDIR` to be set).
 *   - Filesystem entries (files and directories) when completing after a
 *     flag that expects a file path (e.g. `--read` / `-r`).
 *
 * The returned list is pruned to entries that share the current word's
 * prefix, sorted lexicographically, and deduplicated.
 *
 * @param current_word_index  Zero-based index of the word currently being
 *                            completed in the `words` vector.
 * @param words               All tokens of the command line so far
 *                            (e.g. `{"kez", "install", "--env", ""}`).
 *                            The last element may be an empty string when
 *                            the cursor follows a space.
 *
 * @return A sorted, deduplicated vector of completion strings that match
 *         the current word's prefix, or an empty vector if no suggestions
 *         are applicable.
 *
 * @note Depends on the environment variables `KEZ_DB`, `KEZ_HOME`, and
 *       `KEZ_WORKDIR` for context-aware suggestions.  When these are
 *       unset or point to non-existent directories, package- and
 *       environment-related suggestions are silently omitted.
 *
 * @see command_suggestions()  Dispatches to subcommand-specific helpers.
 * @see install_suggestions()  Logic for the `install` subcommand.
 * @see environment_suggestions()  Logic for the `env` subcommand.
 * @see managed_environment_suggestions()  Logic for `compiler` / `mpi`.
 * @see factory_suggestions()  Logic for the `factory` subcommand.
 */
std::vector<std::string> completion_suggestions(int current_word_index,
                                                const std::vector<std::string>& words);
