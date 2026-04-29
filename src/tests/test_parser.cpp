#include <cassert>
#include <parser/filter.hpp>

void test_parser_basic() {
    std::string test_str = "CXXFLAGS=\"-O3 -O2 -O3 -g\"";
    filter(test_str);
    assert(test_str == "CXXFLAGS=\"-O3 -O2 -g\"");
}

int main() {
    INFO("Running parser tests...");
    test_parser_basic();
    SUCCESS("Parser tests passed!");
    return 0;
}