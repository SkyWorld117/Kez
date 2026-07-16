#pragma once

/**
 * @file dump.hpp
 * @brief Template metaprogramming infrastructure for generic value-to-string dumping.
 *
 * Provides a set of type traits, partial specializations, and a dispatcher function
 * that together form a generic "dump" facility.  Any C++ value can be converted to a
 * human-readable string representation: iterable containers are shown as brace-delimited
 * lists, pairs/tuples as parenthesised comma-separated fields, strings and characters
 * are quoted, and all remaining types fall back to operator<< on a std::stringstream.
 *
 * The central entry point is dump(); the bulk of the logic lives in the
 * dump_struct<> family of specializations.
 *
 * @see dump
 * @see dump_struct
 */

#include <map>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

/**
 * @brief Type trait: detects whether a type supports range-based iteration.
 *
 * Inherits from std::true_type if the type has valid std::begin() and std::end()
 * calls, and from std::false_type otherwise.
 *
 * @tparam T The type to test.
 *
 * @see dump_struct  Uses this trait to select the iterable vs. non-iterable specialization.
 */
template <typename T, typename = void> struct is_iterable : std::false_type {};

/// @cond
template <typename T>
struct is_iterable<
    T, std::void_t<decltype(std::begin(std::declval<T>())), decltype(std::end(std::declval<T>()))>>
    : std::true_type {};
/// @endcond

/**
 * @brief Primary template for the value-to-string converter.
 *
 * The second, non-type template parameter `IsArray` selects between two branches:
 * - **true**  — The type is iterable (or is a raw C array).  The generic "iterable"
 *               specialization (dump_struct<T, true>) iterates and formats elements.
 * - **false** — The type is a scalar, a pair, a tuple, or has no standard iteration
 *               interface.  Individual specializations handle pair, tuple, and the
 *               various string/character types; the fallback uses operator<<.
 *
 * @tparam T       The type to convert.
 * @tparam IsArray Boolean flag; true if T is iterable or a raw C array.
 *
 * @see dump Entry point that sets IsArray automatically.
 */
template <typename T, bool IsArray> struct dump_struct {};

/**
 * @brief Dispatcher function that converts an arbitrary value to its string
 *        representation.
 *
 * Delegates to the appropriate dump_struct<T, ...> specialization, setting the
 * `IsArray` flag to `true` when `T` is iterable or a raw C array.
 *
 * @tparam T The (usually deduced) type of the value to dump.
 * @param  value The value to stringify.
 * @return A std::string containing the human-readable representation of @p value.
 *
 * @note This is the main entry point for callers outside the dump infrastructure.
 *
 * @see dump_struct
 */
template <typename T> inline std::string dump(const T& value) {
    return dump_struct<T, is_iterable<T>::value || std::is_array<T>::value> {}(value);
}

/**
 * @brief Specialization of dump_struct for std::pair (non-iterable branch).
 *
 * Formats a pair as `(first, second)`, recursively dumping each element.
 *
 * @tparam T1 Type of the first element.
 * @tparam T2 Type of the second element.
 */
template <typename T1, typename T2> struct dump_struct<std::pair<T1, T2>, false> {
    /**
     * @brief Convert the pair to its string representation.
     * @param value The pair to stringify.
     * @return  A string of the form `(first, second)`.
     */
    std::string operator()(const std::pair<T1, T2>& value) {
        std::string out =
            "(" + dump_struct<T1, is_iterable<T1>::value || std::is_array<T1>::value> {}(
                      std::get<0>(value));
        out += ", " + dump(std::get<1>(value)) + ")";
        return out;
    }
};

/**
 * @brief Specialization of dump_struct for std::tuple (non-iterable branch).
 *
 * Formats a tuple as `(e1, e2, ..., eN)`, recursively dumping each element.
 *
 * @tparam Ts The element types of the tuple.
 */
template <typename... Ts> struct dump_struct<std::tuple<Ts...>, false> {
    /**
     * @brief Recursive helper that dumps every element of the tuple.
     *
     * Uses a fold expression over index_sequence to concatenate the string
     * representations with a `", "` separator.
     *
     * @tparam T  The concrete tuple type (always std::tuple<Ts...>).
     * @tparam Is  A parameter pack of indices 0..(N-1).
     * @param  value The tuple to stringify.
     * @return A string containing the comma-separated dump of all elements.
     */
    template <typename T, std::size_t... Is>
    std::string helper(const T& value, std::index_sequence<Is...>) {
        std::string out = ((dump(std::get<Is>(value)) + ", ") + ... + "");
        out.pop_back();
        out.pop_back();
        return out;
    }

    /**
     * @brief Convert the tuple to its string representation.
     * @param value The tuple to stringify.
     * @return  A string of the form `(e1, e2, ..., eN)`.
     */
    std::string operator()(const std::tuple<Ts...>& value) {
        std::string out = "(";
        out += helper(value, std::index_sequence_for<Ts...> {});

        out += +")";
        return out;
    }
};

/**
 * @brief Specialization of dump_struct for std::string (iterable branch).
 *
 * Overrides the generic iterable formatting so that a string is rendered as a
 * double-quoted literal rather than a brace-delimited list of characters.
 */
template <> struct dump_struct<std::string, true> {
    /**
     * @brief Quote the string with double quotes.
     * @param value The string to quote.
     * @return `"value"`
     */
    std::string operator()(const std::string& value) { return "\"" + value + "\""; }
};

/**
 * @brief Specialization of dump_struct for const char (iterable branch).
 *
 * Handles the degenerate case of a single const char treated as an iterable
 * (a C string of length 1), rendering it as a double-quoted string.
 *
 * @note The IsArray=true specialisation is used when dump deduces
 *       is_iterable<const char>::value || is_array<const char>::value as true.
 *       See also the IsArray=false specialization for char-const.
 */
template <> struct dump_struct<const char, true> {
    /**
     * @brief Quote the character pointer as a double-quoted string.
     * @param value The C-string to quote.
     * @return `"value"`
     */
    std::string operator()(const char* value) {
        return std::string("\"") + value + std::string("\"");
    }
};

/**
 * @brief Specialization of dump_struct for const char[] (non-iterable branch).
 *
 * Handles array-to-pointer decay of string literals so that a literal like
 * `"hello"` is rendered as a quoted string rather than a brace-enclosed char array.
 *
 * @note This covers the case where T is `const char[N]` after array-to-pointer
 *       decay has already occurred, and `IsArray` (the second template param) is
 *       false because the function parameter type is already `const char*`.
 */
template <> struct dump_struct<const char[], false> {
    /**
     * @brief Quote the C-string with double quotes.
     * @param value The string to quote.
     * @return `"value"`
     */
    std::string operator()(const char* value) {
        return std::string("\"") + value + std::string("\"");
    }
};

/**
 * @brief Specialization of dump_struct for char (non-iterable branch).
 *
 * Renders a single char as a single-quoted character literal.
 *
 * @note A char is not considered iterable, so this specialization lives in the
 *       IsArray=false branch.  It correctly handles printable characters; control
 *       characters will be embedded verbatim rather than escaped.
 */
template <> struct dump_struct<char, false> {
    /**
     * @brief Quote a single character with single quotes.
     * @param value The character to quote.
     * @return `'x'`
     */
    std::string operator()(char value) { return std::string("\'") + value + std::string("\'"); }
};

/**
 * @brief Specialization of dump_struct for const char (non-iterable branch).
 *
 * Renders a single const-qualified char as a single-quoted character literal.
 *
 * @note This specialization is reached when T = const char and the type is not
 *       treated as iterable/array.  Together with the IsArray=true counterpart,
 *       both code paths produce the same output but handle different template
 *       instantiation contexts.
 */
template <> struct dump_struct<const char, false> {
    /**
     * @brief Quote a single const character with single quotes.
     * @param value The character to quote.
     * @return `'x'`
     */
    std::string operator()(const char value) {
        return std::string("\'") + value + std::string("\'");
    }
};

/**
 * @brief Specialization of dump_struct for const char* (non-iterable branch).
 *
 * Renders a C-string pointer as a double-quoted string.
 *
 * @note This covers the case where the function argument is an explicit
 *       `const char*` (pointer, not array), for which std::is_array is false.
 */
template <> struct dump_struct<const char*, false> {
    /**
     * @brief Quote the C-string with double quotes.
     * @param value The C-string to quote.
     * @return `"value"`
     */
    std::string operator()(const char* value) {
        return std::string("\"") + value + std::string("\"");
    }
};

/**
 * @brief Generic specialization of dump_struct for iterable / array types.
 *
 * Handles any type T for which is_iterable<T>::value is true, or that is a raw
 * C array.  The elements are formatted via recursive calls to dump() and
 * enclosed in curly braces: `{e1, e2, ..., eN}`.
 *
 * @tparam T The iterable or array type.
 *
 * @note If T exposes multiple sequential iteration passes (e.g. an input-only
 *       iterator), the behaviour is undefined.  The loop copies elements via
 *       `auto& val = *i` — for proxy iterators (e.g. std::vector<bool>) this
 *       may produce unexpected results.
 *
 * @see dump
 */
template <typename T> struct dump_struct<T, true> {
    /**
     * @brief Convert the iterable to a brace-delimited string.
     * @param value The container or array to stringify.
     * @return A string of the form `{e1, e2, ..., eN}`.
     */
    std::string operator()(const T& value) {
        std::string out = "{";
        for (auto i = std::begin(value); i != std::end(value);) {
            auto& val = *i;
            out += dump(val);
            if (++i != std::end(value)) {
                out += ", ";
            }
        }
        out += "}";
        return out;
    }
};

/**
 * @brief Fallback specialization of dump_struct for non-iterable, non-pair,
 *        non-tuple, non-string, non-character types.
 *
 * Uses operator<< on a std::stringstream to produce the string representation.
 * This works for any type that overloads operator<<(std::ostream&, const T&).
 *
 * @tparam T The scalar or user-defined type to stringify.
 *
 * @note If T does not provide a suitable operator<<, the code will fail to
 *       compile with a (usually) noisy template-backtrace error.
 */
template <typename T> struct dump_struct<T, false> {
    /**
     * @brief Convert the value via operator<<.
     * @param value The value to stringify.
     * @return The string produced by streaming @p value into a std::stringstream.
     */
    std::string operator()(const T& value) {
        std::stringstream ss;
        ss << value;
        return ss.str();
    }
};
