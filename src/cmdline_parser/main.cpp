#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "../colors/colored_io.h"
#include "../database/database.h"
#include "../parser/parser.h"
#include "../user_config_generator/user_config_generator.h"
#include "traverse.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        ERROR("Usage: " + std::string(argv[0]) +
              " <package_name> [--config [configs]] [--cellar <cellar>]");
        exit(EXIT_FAILURE);
    }

    // Parse command line arguments
    std::string pkg_name = argv[1];

    std::unordered_map<std::string, std::string> config_options;

    bool has_cellar = false;
    std::string cellar;

    int i               = 2;
    bool parsing_config = false;
    while (i < argc) {
        if (std::string(argv[i]) == "--config" || std::string(argv[i]) == "-c") {
            i++;
            if (i >= argc) {
                ERROR("Missing config value");
                exit(EXIT_FAILURE);
            }
            parsing_config = true;
        } else if (std::string(argv[i]) == "--cellar" || std::string(argv[i]) == "-C") {
            if (parsing_config) {
                parsing_config = false;
                if (config_options.empty()) {
                    ERROR("No configurations specified after --config");
                    exit(EXIT_FAILURE);
                }
            }
            i++;
            if (i >= argc) {
                ERROR("Missing cellar value");
                exit(EXIT_FAILURE);
            }
            has_cellar = true;
            cellar     = argv[i];
            i++;
        } else {
            // Handle configurations in format: <option>=<value>
            std::string option, value;
            size_t pos = std::string(argv[i]).find('=');
            if (pos != std::string::npos) {
                option = std::string(argv[i]).substr(0, pos);
                value  = std::string(argv[i]).substr(pos + 1);
            } else {
                ERROR("Invalid configuration format: " + std::string(argv[i]));
                exit(EXIT_FAILURE);
            }
            config_options[option] = value;
            i++;
        }
    }
    if (parsing_config && config_options.empty()) {
        ERROR("No configurations specified after --config");
        exit(EXIT_FAILURE);
    }

    // Verify cellar
    YAML::Node config    = get_db_config(pkg_name);
    std::string pkg_type = config["cheese"]["type"].as<std::string>();

    if (pkg_type == "compiler" || pkg_type == "mpi" || pkg_type == "vendor") {
        if (has_cellar) {
            ERROR("Cellar is not supported for compiler, MPI, or vendor packages.");
            exit(EXIT_FAILURE);
        }
    } else if (!has_cellar) {
        // If not a compiler, MPI, or vendor package, cellar is required
        cellar = getenv("FROMAGER_CELLAR");
        if (cellar.empty()) {
            ERROR("Cellar must be specified for non-compiler, non-MPI, and "
                  "non-vendor packages.");
            exit(EXIT_FAILURE);
        }
    }

    std::string pkg_version;
    if (config_options.find(pkg_name + ".version") != config_options.end()) {
        pkg_version = config_options[pkg_name + ".version"];
    } else if (config["cheese"]["source"]) {
        pkg_version = config["cheese"]["source"]["releases"][0]["version"].as<std::string>();
    } else {
        ERROR(pkg_name + " should not be installed directly.");
        exit(EXIT_FAILURE);
    }

    if (pkg_type == "compiler") {
        cellar = "compilers/" + pkg_name + "-" + pkg_version;
    } else if (pkg_type == "mpi") {
        std::string compiler_spec;
        if (config_options.find(pkg_name + ".compiler") != config_options.end()) {
            compiler_spec = config_options[pkg_name + ".compiler"];
        } else {
            compiler_spec = "system";  // Default to system compiler
        }
        if (compiler_spec == "system") {
            cellar = "mpis/" + pkg_name + "-" + pkg_version + "-" + compiler_spec;
        } else {
            // Split at `@` (format: <name>@<version>)
            std::string compiler_name    = compiler_spec.substr(0, compiler_spec.find('@'));
            std::string compiler_version = compiler_spec.substr(compiler_spec.find('@') + 1);
            cellar = "mpis/" + pkg_name + "-" + pkg_version + "-" + compiler_name + "-" +
                     compiler_version;
        }
    } else if (pkg_type == "vendor") {
        cellar = "vendors/" + pkg_name + "-" + pkg_version;
    } else {
        std::filesystem::path env_path    = getenv("FROMAGER_ENV");
        std::filesystem::path cellar_path = env_path / cellar;
        if (!std::filesystem::exists(cellar_path)) {
            ERROR("Invalid cellar specified or not found.");
            exit(EXIT_FAILURE);
        }
    }
    std::filesystem::path env_path    = getenv("FROMAGER_ENV");
    std::filesystem::path cellar_path = env_path / cellar;

    // Generate user config
    YAML::Node user_config   = gen_user_config(pkg_name, false);
    YAML::Node starting_node = user_config["cheese"];

    // Modify user config based on command line options
    for (const auto &[key, value] : config_options) {
        traverse(key, value, starting_node);
    }

    // Parse instructions
    YAML::Node instructions = parse(user_config, "release", cellar_path.string());

    // Store instructions in the cellar
    YAML::Emitter out;
    out << instructions;

    std::filesystem::path tmp_path = cellar_path / ".tmp";
    std::filesystem::create_directories(tmp_path);
    std::ofstream ofs((tmp_path / "ins.yaml").string());
    if (!ofs) {
        ERROR("Failed to create instruction file");
        exit(EXIT_FAILURE);
    }
    ofs << out.c_str();
    ofs.close();

    SUCCESS("Instructions written to: " + (tmp_path / "ins.yaml").string());

    std::cout << cellar_path.string() << std::endl;

    return 0;
}
