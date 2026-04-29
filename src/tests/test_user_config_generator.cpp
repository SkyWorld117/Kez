#include <yaml-cpp/yaml.h>

#include <cassert>
#include <user_config_generator/configurations_filter.hpp>
#include <utils/colored_io.hpp>

void test_user_config_generator_basic() {
    YAML::Node config;
    std::vector<std::string> deps = {"dep1"};
    YAML::Node abstract_pkgs;

    YAML::Node res = filtered_configurations(config, deps, abstract_pkgs);
    assert(res.Type() == YAML::NodeType::Map);
    assert(res.size() == 0);
}

int main() {
    INFO("Running user_config_generator tests...");
    test_user_config_generator_basic();
    SUCCESS("user_config_generator tests passed!");
    return 0;
}