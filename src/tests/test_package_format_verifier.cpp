#include <yaml-cpp/yaml.h>

#include <cassert>
#include <package_format_verifier/cheese_verifier.hpp>

void test_package_format_verifier_basic() {
    YAML::Node node;
    node["name"] = "test-pkg";
    node["type"] = "library";

    bool valid = verify_cheese(node);
    assert(valid == true);

    YAML::Node invalid_node;  // empty
    bool invalid_res = verify_cheese(invalid_node);
    assert(invalid_res == false);
}

int main() {
    INFO("Running package format verifier tests...");
    test_package_format_verifier_basic();
    SUCCESS("Package format verifier tests passed!");
    return 0;
}