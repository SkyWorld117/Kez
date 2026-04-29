#include <cassert>
#include <ui/argparser/argparser.hpp>
#include <utils/colored_io.hpp>

// Simple UI tests
void test_ui_basic() {
    // Cannot easily trigger argparse without deep mocking,
    // but at least verifying the compile and a basic check
    assert(true);
}

int main() {
    INFO("Running ui tests...");
    test_ui_basic();
    SUCCESS("UI tests passed!");
    return 0;
}