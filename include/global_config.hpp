#pragma once

#include <yaml-cpp/yaml.h>

#include <string>

std::string get_path(const std::string& target);
std::string get_num_proc();
std::string get_default_compiler();
std::string get_default_mpi();