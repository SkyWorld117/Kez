/**
 * @brief File containing template metaprogramming implementations for the dump functionality
 */
#include <map>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

template <typename T, typename = void> struct is_iterable : std::false_type {};
template <typename T>
struct is_iterable<
    T, std::void_t<decltype(std::begin(std::declval<T>())), decltype(std::end(std::declval<T>()))>>
    : std::true_type {};

template <typename T, bool IsArray> struct dump_struct {};

template <typename T> inline std::string dump_helper(const T& value) {
    return dump_struct < T, is_iterable<T>::value || std::is_array<T>::value > {}(value);
}

template <typename T1, typename T2> struct dump_struct<std::pair<T1, T2>, false> {
    std::string operator()(const std::pair<T1, T2>& value) {
        std::string out = "(" + dump_struct < T1,
                    is_iterable<T1>::value || std::is_array<T1>::value > {}(std::get<0>(value));
        out += ", " + dump_helper(std::get<1>(value)) + ")";
        return out;
    }
};
template <typename... Ts> struct dump_struct<std::tuple<Ts...>, false> {
    template <typename T, std::size_t... Is>
    std::string helper(const T& value, std::index_sequence<Is...>) {
        std::string out = ((dump_helper(std::get<Is>(value)) + ", ") + ... + "");
        out.pop_back();
        out.pop_back();
        return out;
    }

    std::string operator()(const std::tuple<Ts...>& value) {
        std::string out = "(";
        out += helper(value, std::index_sequence_for<Ts...> {});

        out += +")";
        return out;
    }
};
template <> struct dump_struct<std::string, true> {
    std::string operator()(const std::string& value) { return "\"" + value + "\""; }
};
template <> struct dump_struct<const char, true> {
    std::string operator()(const char* value) {
        return std::string("\"") + value + std::string("\"");
    }
};
template <> struct dump_struct<const char[], false> {
    std::string operator()(const char* value) {
        return std::string("\"") + value + std::string("\"");
    }
};

template <> struct dump_struct<char, false> {
    std::string operator()(char value) { return std::string("\'") + value + std::string("\'"); }
};

template <> struct dump_struct<const char, false> {
    std::string operator()(const char value) {
        return std::string("\'") + value + std::string("\'");
    }
};

template <> struct dump_struct<const char*, false> {
    std::string operator()(const char* value) {
        return std::string("\"") + value + std::string("\"");
    }
};

template <typename T> struct dump_struct<T, true> {
    std::string operator()(const T& value) {
        std::string out = "{";
        for (auto i = std::begin(value); i != std::end(value);) {
            auto& val = *i;
            out += dump_helper(val);
            if (++i != std::end(value)) {
                out += ", ";
            }
        }
        out += "}";
        return out;
    }
};

template <typename T> struct dump_struct<T, false> {
    std::string operator()(const T& value) {
        std::stringstream ss;
        ss << value;
        return ss.str();
    }
};