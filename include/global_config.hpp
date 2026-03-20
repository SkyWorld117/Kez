#pragma once

#include <yaml-cpp/yaml.h>

#include <string>

namespace global_config {

    std::string get_path(const std::string& target);
    std::string get_num_proc();
    std::string get_default_compiler();
    std::string get_default_mpi();

}  // namespace global_config

struct CellarPathQuery {
    std::string pkg_name    = "";
    std::string pkg_version = "";
    std::string cellar_name = "";
    // Any package type in this set will trigger an error
    // This will be ignored if `pkg_name` is not provided
    std::set<std::string> type_filters = {};
};

std::string get_cellar_path(CellarPathQuery query);