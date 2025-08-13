#include <yaml-cpp/yaml.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>

#include "../colors/colored_io.h"
#include "user_config_generator.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        ERROR("Usage: " + std::string(argv[0]) + " <package_name> [<output_file>]");
        exit(EXIT_FAILURE);
    }

    std::string pkg_name = argv[1];
    YAML::Node user_config = gen_user_config(pkg_name);

    // Output the generated configuration
    YAML::Emitter out;
    out << user_config;

    std::cout << out.c_str() << std::endl;

    // If an output file is specified, write the configuration to it
    if (argc == 3) {
        std::string output_file = argv[2];
        std::ofstream ofs(output_file);
        if (!ofs) {
            ERROR("Could not open output file: " + output_file);
            exit(EXIT_FAILURE);
        }
        ofs << out.c_str();
        ofs.close();
        SUCCESS("Configuration written to: " + output_file);
    } else {
        SUCCESS("Configuration output to stdout.");
    }

    return 0;
}