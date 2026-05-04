#include <cassert>
#include <cmdline_parser/traverse.hpp>
#include <utils/colored_io.hpp>

void test_traverse_basic() {
    YAML::Node root;
    root["some"]["path"] = "old";
    traverse("some.path", "value", root);
    assert(root["some"]["path"].as<std::string>() == "value");
}

void test_traverse_deep() {
    YAML::Node root;
    root["a"]["b"]["c"]["d"] = "456";
    traverse("a.b.c.d", "123", root);
    assert(root["a"]["b"]["c"]["d"].as<int>() == 123);
}

void test_traverse_existing() {
    YAML::Node root;
    root["a"]["b"] = "old";
    root["a"]["c"] = "old2";
    traverse("a.c", "new", root);
    assert(root["a"]["b"].as<std::string>() == "old");
    assert(root["a"]["c"].as<std::string>() == "new");
}

int main() {
    INFO("Running cmdline parser tests...");
    test_traverse_basic();
    test_traverse_deep();
    test_traverse_existing();
    SUCCESS("Cmdline parser tests passed!");
    return 0;
}