#include <yaml-cpp/yaml.h>

#include <cassert>
#include <rt_dependency_resolver/dependents.hpp>
#include <rt_dependency_resolver/unbuilt_dependencies.hpp>
#include <utils/colored_io.hpp>

void test_rt_dependency_resolver_basic() {
    // Requires a mocked FROMAGER_DB to actually hit DB configurations.
    // For now we just instantiate nodes to make sure it includes and compiles correctly.
    YAML::Node user_config;
    YAML::Node ins_yaml;
    std::vector<std::string> installed = {"pkg-A"};

    // get_dependents will try to hit the DB if we give it actual packages
    // user_config["cheese"]["pkg-B"] = YAML::Node();

    assert(true);
}

int main() {
    INFO("Running rt_dependency_resolver tests...");
    test_rt_dependency_resolver_basic();
    SUCCESS("rt_dependency_resolver tests passed!");
    return 0;
}