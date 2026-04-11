#include <colors/colored_io.hpp>
#include <database/database.hpp>
#include <filesystem>
#include <global_config.hpp>
#include <parser/scalar_parser.hpp>
#include <string>

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

    std::string get_external_package_prefix(const std::string& package_name) {
        if (!config["external"] || !config["external"][package_name] ||
            !config["external"][package_name]["prefix"]) {
            ERROR("Illformed configuration: prefix for external package '" + package_name +
                  "' is missing");
            exit(EXIT_FAILURE);
        } else {
            return config["external"][package_name]["prefix"].as<std::string>();
        }
    }

    std::string get_external_package_version(const std::string& package_name) {
        if (!config["external"] || !config["external"][package_name] ||
            !config["external"][package_name]["version"]) {
            ERROR("Illformed configuration: version for external package '" + package_name +
                  "' is missing");
            exit(EXIT_FAILURE);
        } else {
            return config["external"][package_name]["version"].as<std::string>();
        }
    }

}  // namespace global_config

void filter(const std::set<std::string>& type_filters, const std::string& pkg_type,
            const std::string& pkg_names) {
    if (!type_filters.empty() && type_filters.find(pkg_type) != type_filters.end()) {
        ERROR("Packages '" + pkg_names + "' are of type '" + pkg_type +
              "', which is not allowed by the type filters.");
        exit(EXIT_FAILURE);
    }
}

std::string get_pkg_type(const std::vector<std::string>& pkg_names) {
    std::string pkg_type = "";
    for (const std::string& pkg_name : pkg_names) {
        YAML::Node pkg_config        = get_db_config(pkg_name);
        std::string current_pkg_type = pkg_config["cheese"]["type"].as<std::string>();
        if (pkg_type.empty()) {
            pkg_type = current_pkg_type;
        } else if (pkg_type != current_pkg_type) {
            ERROR("All packages in the query should be of the same type. Package '" + pkg_name +
                  "' is of type '" + current_pkg_type +
                  "', which is different from the previous packages of type '" + pkg_type + "'.");
            exit(EXIT_FAILURE);
        }
    }
    if (pkg_type == "compiler" || pkg_type == "mpi" || pkg_type == "vendor" ||
        pkg_type == "external") {
        if (pkg_names.size() > 1) {
            ERROR("There should be at most one package of type '" + pkg_type +
                  "' in the query due to the way cellar paths are structured for these types.");
            exit(EXIT_FAILURE);
        }
    }
    return pkg_type;
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
        // When package names are provided, we need to identify if the query is legal
        // - All packages in the query should have the same type
        // - If the type is "compiler", "mpi", "vendor", or "external", there should be at most one package in the query. This is due to the fact that the cellar path for these types is uniquely determined by the package name, version, and (for mpi) compiler spec.
        std::string pkg_type      = get_pkg_type(query.pkg_name);
        std::string pkg_names_str = "";
        for (const std::string& pkg_name : query.pkg_name) {
            pkg_names_str += pkg_name + " ";
        }
        pkg_names_str = pkg_names_str.substr(0, pkg_names_str.size() - 1);  // Remove trailing space
        filter(query.type_filters, pkg_type, pkg_names_str);

        if (pkg_type == "system") {
            if (!query.cellar_name.empty() && query.cellar_name != "system") {
                ERROR("System package '" + pkg_names_str + "' should be in the 'system' cellar.");
                exit(EXIT_FAILURE);
            }
            return global_config::get_path("system");
        } else if (pkg_type == "compiler" || pkg_type == "mpi" || pkg_type == "vendor") {
            if (pkg_type == "vendor") {
                YAML::Node pkg_config = get_db_config(query.pkg_name[0]);
                if (pkg_config["cheese"]["properties"] &&
                    pkg_config["cheese"]["properties"]["prefix"]) {
                    // This means it is a submodule of another vendor package
                    CellarPathQuery prefix_query;
                    if (pkg_config["cheese"]["properties"]["parent"]) {
                        prefix_query.pkg_name = {
                            pkg_config["cheese"]["properties"]["parent"].as<std::string>()};
                    } else {
                        ERROR("Vendor package '" + query.pkg_name[0] +
                              "' has a prefix property but no parent property to indicate which "
                              "package it belongs to.");
                        exit(EXIT_FAILURE);
                    }
                    prefix_query.pkg_version = query.pkg_version;

                    std::string parent_prefix = get_cellar_path(prefix_query);

                    // Possible patterns to substitute in the prefix property:
                    // - ${parent_pkg.version}: the version of the current package
                    // - ${parent_pkg.prefix}: the prefix of the parent package
                    // TODO: Sophia parse architecture here :)
                    std::string prefix_template =
                        pkg_config["cheese"]["properties"]["prefix"].as<std::string>();
                    std::string template_parent_pkg_version =
                        "${" + prefix_query.pkg_name[0] + ".version}";
                    std::string template_parent_pkg_prefix =
                        "${" + prefix_query.pkg_name[0] + ".prefix}";
                    if (prefix_template.find(template_parent_pkg_version) != std::string::npos) {
                        prefix_template.replace(prefix_template.find(template_parent_pkg_version),
                                                template_parent_pkg_version.size(),
                                                query.pkg_version);
                    }
                    if (prefix_template.find(template_parent_pkg_prefix) != std::string::npos) {
                        prefix_template.replace(prefix_template.find(template_parent_pkg_prefix),
                                                template_parent_pkg_prefix.size(), parent_prefix);
                    }

                    return prefix_template;
                }
            }

            std::string expected_cellar = pkg_type + "s";
            if (!query.cellar_name.empty() && query.cellar_name != expected_cellar) {
                ERROR("Package '" + pkg_names_str + "' of type '" + pkg_type +
                      "' should be in the '" + expected_cellar + "' cellar.");
                exit(EXIT_FAILURE);
            }
            if (query.pkg_version.empty()) {
                ERROR("Version is required for package '" + pkg_names_str +
                      "' to determine the cellar path.");
                exit(EXIT_FAILURE);
            }
            // If version is "system", we return the system cellar path; otherwise, we return the path to the specific package version in the expected cellar
            // This is mostly used for the compilers because we prepare a default compiler in the system cellar, but we still want to allow users to specify compiler versions in the config file and have them installed in the compilers cellar
            if (query.pkg_version == "system") {
                return global_config::get_path("system");
            } else if (pkg_type == "mpi") {
                // For MPI packages, we need to handle the prefix differently
                // The goal is to share MPI libraries across the environments
                if (query.compiler_spec.empty()) {
                    ERROR(
                        "Compiler specification (name and version) is required for MPI package '" +
                        pkg_names_str + "' to determine the cellar path.");
                    exit(EXIT_FAILURE);
                }
                return global_config::get_path("mpis") + "/" + query.pkg_name[0] + "-" +
                       query.pkg_version + "-" + compiler_spec_to_cellar_name(query.compiler_spec);
            } else {
                return global_config::get_path(expected_cellar) + "/" + query.pkg_name[0] + "-" +
                       query.pkg_version;
            }
        } else if (pkg_type == "external") {
            return global_config::get_external_package_prefix(query.pkg_name[0]);
        } else {
            if (query.cellar_name.empty()) {
                ERROR("Cellar name is required for package '" + pkg_names_str + "' of type '" +
                      pkg_type + "'.");
                exit(EXIT_FAILURE);
            }
            return global_config::get_path("cellars") + "/" + query.cellar_name;
        }
    }
}

std::pair<std::string, std::string> parse_compiler_spec(const std::string& compiler_spec) {
    if (compiler_spec == "system") {
        return {"gcc", "system"};  // Default to gcc for system compiler
    } else if (compiler_spec.find('@') != std::string::npos) {
        std::string compiler_name    = compiler_spec.substr(0, compiler_spec.find('@'));
        std::string compiler_version = compiler_spec.substr(compiler_spec.find('@') + 1);
        return {compiler_name, compiler_version};
    } else {
        ERROR("Invalid compiler specification: " + compiler_spec +
              ". Expected format is 'compiler@version' or 'system'.");
        exit(EXIT_FAILURE);
    }
}

std::string compiler_spec_to_cellar_name(const std::string& compiler_spec) {
    auto [compiler_name, compiler_version] = parse_compiler_spec(compiler_spec);
    if (compiler_name == "system") {
        return "system";
    } else {
        return compiler_name + "-" + compiler_version;
    }
}