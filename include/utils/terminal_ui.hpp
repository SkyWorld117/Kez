#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

/**
 * @brief Display a keyboard-driven checklist and return the checked entries.
 *
 * Arrow keys or @c j/@c k move the cursor, Space toggles an entry, and Enter
 * confirms the checklist.  Redirected input falls back to one y/n prompt per
 * entry.
 *
 * @param prompt    Heading displayed immediately above the checklist.
 * @param entries   Labels to display in checklist order.
 * @param selected  Initial checked state.  An empty vector means all entries
 *                  start unchecked.
 * @return One boolean per entry, preserving @p entries order.
 *
 * @warning Terminates through @c ERROR if @p selected has the wrong size or
 *          input ends before confirmation.
 */
std::vector<bool> terminal_select_multiple(const std::string& prompt,
                                           const std::vector<std::string>& entries,
                                           const std::vector<bool>& selected = {});

/**
 * @brief Display a mutually-exclusive selector with an optional empty choice.
 *
 * Navigation is identical to terminal_select_multiple(), but at most one
 * entry may be checked.  Pressing Space on the checked entry clears it.  In
 * redirected mode the user enters an entry label; an empty line selects none.
 *
 * @return The selected entry index, or @c std::nullopt when none was selected.
 */
std::optional<std::size_t> terminal_select_one(const std::string& prompt,
                                               const std::vector<std::string>& entries);
