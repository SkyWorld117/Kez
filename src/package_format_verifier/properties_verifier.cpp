#include "properties_verifier.h"

bool verify_flags(const YAML::Node& node) {
    if (!node.IsMap()) {
        ERROR("Error at '" + std::to_string(node.Mark().line) + "':\n"
              "Flags must be a map.");
        return false;
    }
    if (node["default"] && !node["default"].IsScalar()) {
        ERROR("Error at '" + std::to_string(node.Mark().line) + "':\n"
              "The 'default' key must be a scalar value.");
        return false;
    }
    if (node["conditions"] && !verify_conditions(node["conditions"])) {
        return false;
    }
    if (!node["default"] && !node["conditions"]) {
        WARNING("At '" + std::to_string(node.Mark().line) + "':\n"
                "Flags should have either a 'default' value or 'conditions', otherwise consider removing this section.");
    }

    return true;
}

bool verify_properties(const YAML::Node& node, const std::string& pkg_type) {
    if (!node.IsMap()) {
        ERROR("Properties must be a map.");
        return false;
    }

    if (pkg_type == "compiler" || pkg_type == "mpi") {
        if (node["c"] && !node["c"].IsScalar()) {
            ERROR("Compiler options must be a scalar.");
            return false;
        }
        if (node["cxx"] && !node["cxx"].IsScalar()) {
            ERROR("C++ compiler options must be a scalar.");
            return false;
        }
        if (node["fort"] && !node["fort"].IsScalar()) {
            ERROR("Fortran compiler options must be a scalar.");
            return false;
        }
        if (node["omp_flags"] && !node["omp_flags"].IsScalar()) {
            ERROR("OpenMP flags must be a scalar.");
            return false;
        }
    }

    if (node["cflags"] && !verify_flags(node["cflags"])) {
        return false;
    }
    if (node["cxxflags"] && !verify_flags(node["cxxflags"])) {
        return false;
    }
    if (node["fortflags"] && !verify_flags(node["fortflags"])) {
        return false;
    }
    if (node["ldflags"] && !verify_flags(node["ldflags"])) {
        return false;
    }
    if (node["libs"] && !verify_flags(node["libs"])) {
        return false;
    }
    if (node["includes"] && !verify_flags(node["includes"])) {
        return false;
    }

    return true;
}