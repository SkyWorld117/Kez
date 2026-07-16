/**
 * @file dependency_edges.cpp
 * @brief Dependency-edge generation for executable package plans.
 */

#include <dependency_resolver/requirements.hpp>
#include <string>
#include <uconf_parser/parser_internal.hpp>
#include <unordered_set>
#include <utils/string_utils.hpp>
#include <vector>

namespace {

    std::string plan_dependency_name(const std::string& dependency,
                                     const UserConfigParserContext& context) {
        const auto selected = context.abstract_packages.find(dependency);
        const std::string resolved =
            selected == context.abstract_packages.end() ? dependency : selected->second;
        const auto alias = context.package_aliases.find(resolved);
        return alias == context.package_aliases.end() ? resolved : alias->second;
    }

    void collect_buildable_dependencies(const std::string& dependency,
                                        const UserConfigParserContext& context,
                                        const std::unordered_set<std::string>& buildable_packages,
                                        std::unordered_set<std::string>& visited,
                                        std::vector<std::string>& result) {
        const std::string resolved = plan_dependency_name(dependency, context);
        if (!visited.insert(resolved).second) {
            return;
        }
        if (buildable_packages.find(resolved) != buildable_packages.end()) {
            append_unique(result, resolved);
            return;
        }
        const auto parsed = context.package_indices.find(resolved);
        if (parsed == context.package_indices.end()) {
            return;
        }
        for (const Dependency& sub_dependency :
             context.packages[parsed->second].database_config->dependencies) {
            collect_buildable_dependencies(sub_dependency.name, context, buildable_packages,
                                           visited, result);
        }
    }

    void append_plan_requirements(const std::vector<std::string>& requirements,
                                  const UserConfigParserContext& context,
                                  const std::unordered_set<std::string>& buildable_packages,
                                  std::unordered_set<std::string>& visited,
                                  std::vector<std::string>& result) {
        if (!requirements_satisfied(requirements, context.dependencies,
                                    context.abstract_packages)) {
            return;
        }
        for (const std::string& requirement : requirements) {
            collect_buildable_dependencies(requirement, context, buildable_packages, visited,
                                           result);
        }
    }

    void append_plan_configuration_dependencies(
        const BuildConfiguration& configuration, const UserConfigParserContext& context,
        const std::unordered_set<std::string>& buildable_packages,
        std::unordered_set<std::string>& visited, std::vector<std::string>& result) {
        for (const EnvironmentVariable& variable : configuration.environment) {
            append_plan_requirements(variable.requires, context, buildable_packages, visited,
                                     result);
        }
        for (const BuildOption& option : configuration.options) {
            append_plan_requirements(option.requires, context, buildable_packages, visited, result);
        }
    }

}  // namespace

std::vector<std::string> generate_package_dependencies(
    const ParsedUserPackage& package, const UserConfigParserContext& context,
    const std::unordered_set<std::string>& buildable_packages) {
    std::vector<std::string> result;
    std::unordered_set<std::string> visited;
    for (const Dependency& dependency : package.database_config->dependencies) {
        collect_buildable_dependencies(dependency.name, context, buildable_packages, visited,
                                       result);
    }

    if (!package.transformed_build.has_value()) {
        return result;
    }
    const Build& build = *package.transformed_build;
    if (build.configurations.has_value()) {
        append_plan_configuration_dependencies(*build.configurations, context, buildable_packages,
                                               visited, result);
    }
    for (const BuildStage& stage : build.stages) {
        if (stage.configurations.has_value()) {
            append_plan_configuration_dependencies(*stage.configurations, context,
                                                   buildable_packages, visited, result);
        }
    }
    return result;
}
