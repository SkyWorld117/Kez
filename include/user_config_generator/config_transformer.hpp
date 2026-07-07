#pragma once

#include <database/config.hpp>
#include <dependency_resolver/resolve_dependencies.hpp>
#include <optional>
#include <string>
#include <unordered_set>

namespace user_config_generator {

    BuildConfiguration transformed_configuration(
        const BuildConfiguration& configuration, const PackageConfig& package, Toolchain toolchain,
        const std::unordered_set<std::string>& dependencies,
        const AbstractPackageSelections& abstract_packages, const std::string& compiler);

    std::optional<Build> transformed_build(const PackageConfig& package,
                                           const std::unordered_set<std::string>& dependencies,
                                           const AbstractPackageSelections& abstract_packages,
                                           const std::string& compiler);

}  // namespace user_config_generator
