#include <yaml-cpp/yaml.h>

#include <cassert>
#include <cstdlib>
#include <database/database.hpp>
#include <filesystem>
#include <fstream>
#include <utils/colored_io.hpp>

void test_database_init() {
    std::filesystem::path current_dir = std::filesystem::current_path();
    std::filesystem::path db_path     = current_dir / "test_db";
    std::filesystem::create_directory(db_path);

    std::filesystem::path fake_yaml = db_path / "fake_pkg.yaml";
    std::ofstream out(fake_yaml);
    out << "cheese: { name: fake_pkg }\n";
    out.close();

    setenv("FROMAGER_DB", db_path.c_str(), 1);

    YAML::Node config = get_db_config("fake_pkg");
    assert(config["cheese"]["name"].as<std::string>() == "fake_pkg");

    std::filesystem::remove_all(db_path);
}

int main() {
    INFO("Running database tests...");
    test_database_init();
    SUCCESS("Database tests passed!");
    return 0;
}