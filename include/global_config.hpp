#pragma once

#include <yaml-cpp/yaml.h>

#include <string>

namespace global_config {

    std::string get_path(const std::string& target);
    std::string get_num_proc();
    std::string get_default_compiler();
    std::string get_default_mpi();
    std::string get_external_package_prefix(const std::string& package_name);
    std::string get_external_package_version(const std::string& package_name);

}  // namespace global_config

struct CellarPathQuery {
    std::string pkg_name      = "";
    std::string pkg_version   = "";
    std::string cellar_name   = "";
    std::string compiler_spec = "";  // Used for MPI packages
    // Any package type in this set will trigger an error
    // This will be ignored if `pkg_name` is not provided
    std::set<std::string> type_filters = {};
};

std::string get_cellar_path(CellarPathQuery query);

std::pair<std::string, std::string> parse_compiler_spec(const std::string& compiler_spec);
std::string compiler_spec_to_cellar_name(const std::string& compiler_spec);