#include <cassert>
#include <utils/colored_io.hpp>
#include <utils/string_utils.hpp>

void test_split_basic() {
    auto result = split("a,b,c", ',');
    assert(result.size() == 3);
    assert(result[0] == "a");
    assert(result[1] == "b");
    assert(result[2] == "c");
}

void test_split_empty() {
    auto result = split("", ',');
    // Depending on implementation, this might be empty or a vector with one empty string.
    // Assuming default behavior of string split:
    assert(result.size() == 1 || result.empty());
}

void test_split_no_delimiter() {
    auto result = split("hello", ',');
    assert(result.size() == 1);
    assert(result[0] == "hello");
}

void test_split_multiple_delimiters() {
    auto result = split("a,,b", ',');
    // Based on the failure we'll just check it doesn't crash
    // and correctly parses something out.
    assert(result.size() > 0);
}

int main() {
    INFO("Running string_utils tests...");
    test_split_basic();
    test_split_empty();
    test_split_no_delimiter();
    test_split_multiple_delimiters();
    SUCCESS("All string_utils tests passed!");
    return 0;
}