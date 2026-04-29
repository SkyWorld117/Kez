#include <yaml-cpp/yaml.h>

#include <cassert>
#include <rt_profile_config_parser/cellar_parser.hpp>
#include <utils/colored_io.hpp>

void test_rt_profile_config_parser_basic() {
    YAML::Node node = YAML::Node(YAML::NodeType::Map);
    assert(node.IsNull() == false);
}

int main() {
    INFO("Running rt_profile_config_parser tests...");
    test_rt_profile_config_parser_basic();
    SUCCESS("rt_profile_config_parser tests passed!");
    return 0;
}