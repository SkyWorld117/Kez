#include <cassert>
#include <global_config.hpp>
#include <utils/colored_io.hpp>

void test_get_num_proc() {
    std::string num_proc = global_config::get_num_proc();
    assert(!num_proc.empty());
}

void test_get_default_compiler() {
    std::string compiler = global_config::get_default_compiler();
    assert(!compiler.empty());  // typically should have a default
}

void test_parse_compiler_spec() {
    auto [name, version] = parse_compiler_spec("gcc@11.2.0");
    assert(name == "gcc");
    assert(version == "11.2.0");
}

void test_compiler_spec_to_cellar_name() {
    std::string cellar_name = compiler_spec_to_cellar_name("gcc@11.2.0");
    assert(cellar_name == "gcc-11.2.0");
}

int main() {
    INFO("Running global config tests...");
    test_get_num_proc();
    test_get_default_compiler();
    test_parse_compiler_spec();
    test_compiler_spec_to_cellar_name();
    SUCCESS("Global config tests passed!");
    return 0;
}