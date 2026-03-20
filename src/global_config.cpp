#include <colors/colored_io.hpp>
#include <database/database.hpp>
#include <filesystem>
#include <global_config.hpp>

std::filesystem::path prefix_path = std::filesystem::path(getenv("FROMAGER_WORKDIR"));
std::filesystem::path config_path = prefix_path / "config.yaml";
YAML::Node config                 = YAML::LoadFile(config_path.string())["fromager"];

namespace global_config {

    std::string get_path(const std::string& target) {
        if (!config["paths"]) {
            ERROR("Illformed configuration: 'paths' section is missing");
            exit(EXIT_FAILURE);
        } else if (!config["paths"][target]) {
            ERROR("Illformed configuration: path for '" + target + "' is missing");
            exit(EXIT_FAILURE);
        } else {
            return (prefix_path / config["paths"][target].as<std::string>()).string();
        }
    }

    std::string get_num_proc() {
        if (!config["n_proc_for_build"]) {
            ERROR("Illformed configuration: 'n_proc_for_build' is missing");
            exit(EXIT_FAILURE);
        } else {
            return config["n_proc_for_build"].as<std::string>();
        }
    }

    std::string get_default_compiler() {
        if (!config["default_compiler"]) {
            ERROR("Illformed configuration: 'default_compiler' is missing");
            exit(EXIT_FAILURE);
        } else {
            return config["default_compiler"].as<std::string>();
        }
    }

    std::string get_default_mpi() {
        if (!config["default_mpi"]) {
            ERROR("Illformed configuration: 'default_mpi' is missing");
            exit(EXIT_FAILURE);
        } else {
            return config["default_mpi"].as<std::string>();
        }
    }

}  // namespace global_config

void filter(const std::set<std::string>& type_filters, const std::string& pkg_type,
            const std::string& pkg_name) {
    if (!type_filters.empty() && type_filters.find(pkg_type) != type_filters.end()) {
        ERROR("Package '" + pkg_name + "' is of type '" + pkg_type +
              "', which is not allowed by the type filters.");
        exit(EXIT_FAILURE);
    }
}

std::string get_cellar_path(CellarPathQuery query) {
    if (query.pkg_name.empty()) {
        if (query.cellar_name.empty()) {
            ERROR("Both package name and cellar name are missing in the query.");
            exit(EXIT_FAILURE);
        } else if (query.cellar_name == "system" || query.cellar_name == "utilities") {
            return global_config::get_path(query.cellar_name);
        } else if (query.cellar_name == "compilers" || query.cellar_name == "mpis" ||
                   query.cellar_name == "vendors") {
            ERROR("Cellars for compilers, MPIs, and vendors should be accessed through package "
                  "queries with version specified, not directly by cellar name.");
            exit(EXIT_FAILURE);
        } else {
            return global_config::get_path("cellars") + "/" + query.cellar_name;
        }
    } else {
        YAML::Node pkg_config = get_db_config(query.pkg_name);
        std::string pkg_type  = pkg_config["cheese"]["type"].as<std::string>();
        filter(query.type_filters, pkg_type, query.pkg_name);

        if (pkg_type == "system") {
            if (!query.cellar_name.empty() && query.cellar_name != "system") {
                ERROR("System package '" + query.pkg_name + "' should be in the 'system' cellar.");
                exit(EXIT_FAILURE);
            }
            return global_config::get_path("system");
        } else if (pkg_type == "compiler" || pkg_type == "mpi" || pkg_type == "vendor") {
            std::string expected_cellar = pkg_type + "s";
            if (!query.cellar_name.empty() && query.cellar_name != expected_cellar) {
                ERROR("Package '" + query.pkg_name + "' of type '" + pkg_type +
                      "' should be in the '" + expected_cellar + "' cellar.");
                exit(EXIT_FAILURE);
            }
            if (query.pkg_version.empty()) {
                ERROR("Version is required for package '" + query.pkg_name +
                      "' to determine the cellar path.");
                exit(EXIT_FAILURE);
            }
            // If version is "system", we return the system cellar path; otherwise, we return the path to the specific package version in the expected cellar
            // This is mostly used for the compilers because we prepare a default compiler in the system cellar, but we still want to allow users to specify compiler versions in the config file and have them installed in the compilers cellar
            return query.pkg_version == "system" ? global_config::get_path("system")
                                                 : global_config::get_path(expected_cellar) + "/" +
                                                       query.pkg_name + "-" + query.pkg_version;
        } else {
            if (query.cellar_name.empty()) {
                ERROR("Cellar name is required for package '" + query.pkg_name + "' of type '" +
                      pkg_type + "'.");
                exit(EXIT_FAILURE);
            }
            return global_config::get_path("cellars") + "/" + query.cellar_name;
        }
    }
}