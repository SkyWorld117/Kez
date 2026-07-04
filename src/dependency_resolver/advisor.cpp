#include <yaml-cpp/yaml.h>

#include <cstdlib>
#include <dependency_resolver/advisor.hpp>
#include <filesystem>
#include <utils/bash_utils.hpp>
#include <utils/colored_io.hpp>

std::string advise(const std::string& abstract_package) {
    const std::string kez_home     = get_env_var("KEZ_HOME");
    const std::string architecture = get_env_var("KEZ_ARCH");

    const std::filesystem::path advice_path =
        std::filesystem::path(kez_home) / "heuristics" / "advice.yaml";

    const YAML::Node advice         = YAML::LoadFile(advice_path.string())["advice"];
    const YAML::Node recommendation = advice[abstract_package][architecture];
    if (recommendation && recommendation.IsScalar()) {
        return recommendation.as<std::string>();
    }

    ERROR("No dependency advice for package '" + abstract_package + "' on architecture '" +
          architecture + "'");
    exit(EXIT_FAILURE);
}
