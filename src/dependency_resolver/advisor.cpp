#include <yaml-cpp/yaml.h>

#include <cstdlib>
#include <dependency_resolver/advisor.hpp>
#include <filesystem>
#include <utils/bash_utils.hpp>
#include <utils/colored_io.hpp>
#include <utils/yaml_utils.hpp>

/** @brief Looks up the concrete package substitution for an abstract package in the
 *         heuristics/advice.yaml table, keyed by the current KEZ_ARCH architecture.
 *         Terminates with an error if no matching advice entry is found. */
std::string advise(const std::string& abstract_package) {
    const std::string kez_home     = get_env_var("KEZ_HOME");
    const std::string architecture = get_env_var("KEZ_ARCH");

    const std::filesystem::path advice_path =
        std::filesystem::path(kez_home) / "heuristics" / "advice.yaml";

    const YAML::Node advice = cached_yaml_load(advice_path)["advice"];
    if (yaml_has(advice, abstract_package) && yaml_has(advice[abstract_package], architecture)) {
        return yaml_scalar(advice[abstract_package][architecture],
                           "dependency advice for " + abstract_package);
    }

    ERROR("No dependency advice for package '" + abstract_package + "' on architecture '" +
          architecture + "'");
    exit(EXIT_FAILURE);
}
