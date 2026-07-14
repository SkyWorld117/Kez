/**
 * @file configuration_evaluator.cpp
 * @brief Fixed-point evaluation of user-configurable options and environment values.
 */

#include <dependency_resolver/requirements.hpp>
#include <parser/parser_internal.hpp>
#include <string>
#include <utils/yaml_utils.hpp>

namespace {

    std::string configurable_user_string(
        const YAML::Node& user_value, const std::string& key,
        const std::optional<ConfigurableValue<std::string>>& value) {
        if (user_value.IsDefined() && yaml_has(user_value, key)) {
            return user_value[key].IsNull()
                       ? ""
                       : yaml_scalar(user_value[key], "option field '" + key + "'");
        }
        return value.has_value() ? value->default_value.value_or("") : "";
    }

    bool option_state_equal(const ParsedOptionState& left, const ParsedOptionState& right) {
        return left.enabled == right.enabled && left.enabled_value == right.enabled_value &&
               left.disabled_value == right.disabled_value;
    }

    void compute_environment(const BuildConfiguration& configuration,
                             const UserConfigurationIndex& user_configuration,
                             const PackageConfig& package, UserConfigParserContext& context,
                             bool use_user_values, bool apply_conditions) {
        for (const EnvironmentVariable& variable : configuration.environment) {
            const bool required = requirements_satisfied(variable.requires, context.dependencies,
                                                         context.abstract_packages);
            std::string value;
            if (required && variable.user_configurable && use_user_values) {
                const auto user_value = user_configuration.environment.find(variable.name);
                if (user_value == user_configuration.environment.end()) {
                    user_config_error("package '" + package.name +
                                      "' is missing configurable "
                                      "environment variable '" +
                                      variable.name + "'");
                }
                value =
                    yaml_has(user_value->second, "value") && !user_value->second["value"].IsNull()
                        ? yaml_scalar(user_value->second["value"], "environment variable value")
                        : "";
            } else if (required) {
                value = variable.value.default_value.value_or("");
            }
            if (required && apply_conditions) {
                value = apply_parser_conditions(variable.value, value, context);
            }
            const std::string env_key = package.name + ".env." + variable.name;
            const auto previous_env   = context.named_environment_values.find(env_key);
            if (previous_env == context.named_environment_values.end() ||
                previous_env->second != value) {
                ++context.config_version;
            }
            context.environment_values[&variable]     = value;
            context.named_environment_values[env_key] = value;
        }
    }

    void compute_options(const BuildConfiguration& configuration,
                         const UserConfigurationIndex& user_configuration,
                         const PackageConfig& package, UserConfigParserContext& context,
                         bool use_user_values, bool apply_conditions) {
        for (const BuildOption& option : configuration.options) {
            const bool required = requirements_satisfied(option.requires, context.dependencies,
                                                         context.abstract_packages);
            YAML::Node user_value;
            if (required && option.user_configurable && use_user_values) {
                const auto indexed_value = user_configuration.options.find(option.name);
                if (indexed_value == user_configuration.options.end()) {
                    user_config_error("package '" + package.name +
                                      "' is missing configurable option '" + option.name + "'");
                }
                user_value = indexed_value->second;
            }

            ParsedOptionState state;
            if (!required) {
                state.enabled = false;
            } else if (option.user_configurable && use_user_values) {
                if (yaml_has(user_value, "enabled")) {
                    state.enabled = yaml_boolean(user_value["enabled"], "option enabled value");
                } else if (option.enabled.has_value() && !option.enabled->conditions.empty()) {
                    state.enabled = option.enabled->default_value.value_or(false);
                } else {
                    user_config_error("package '" + package.name + "' option '" + option.name +
                                      "' is missing its enabled value");
                }
            } else {
                state.enabled = option.enabled.has_value()
                                    ? option.enabled->default_value.value_or(true)
                                    : true;
            }

            if (required && option.enabled.has_value() && apply_conditions) {
                state.enabled = apply_parser_conditions(*option.enabled, state.enabled, context);
            }
            const std::string key = package.name + ".config." + option.name;

            const auto preexisting = context.named_option_values.find(key);
            const bool is_new      = preexisting == context.named_option_values.end();
            const ParsedOptionState previous_state =
                is_new ? ParsedOptionState {} : preexisting->second;

            context.named_option_values[key] = state;

            if (required && option.user_configurable && use_user_values) {
                state.enabled_value =
                    configurable_user_string(user_value, "enabled_value", option.enabled_value);
                state.disabled_value =
                    configurable_user_string(user_value, "disabled_value", option.disabled_value);
            } else if (required) {
                state.enabled_value  = option.enabled_value.has_value()
                                           ? option.enabled_value->default_value.value_or("")
                                           : "";
                state.disabled_value = option.disabled_value.has_value()
                                           ? option.disabled_value->default_value.value_or("")
                                           : "";
            }
            if (required && option.enabled_value.has_value() && apply_conditions) {
                state.enabled_value =
                    apply_parser_conditions(*option.enabled_value, state.enabled_value, context);
            }
            if (required && option.disabled_value.has_value() && apply_conditions) {
                state.disabled_value =
                    apply_parser_conditions(*option.disabled_value, state.disabled_value, context);
            }
            if (is_new || !option_state_equal(previous_state, state)) {
                ++context.config_version;
            }
            context.option_values[&option]   = state;
            context.named_option_values[key] = state;
        }
    }

    void compute_configuration(const BuildConfiguration& configuration,
                               const UserConfigurationIndex& user_configuration,
                               const PackageConfig& package, UserConfigParserContext& context,
                               bool use_user_values, bool apply_conditions) {
        compute_environment(configuration, user_configuration, package, context, use_user_values,
                            apply_conditions);
        compute_options(configuration, user_configuration, package, context, use_user_values,
                        apply_conditions);
    }

    bool compute_values(UserConfigParserContext& context, bool apply_conditions) {
        const std::size_t previous_version = context.config_version;

        for (const ParsedUserPackage& package : context.packages) {
            context.current_package = package.requested_name;
            if (!package.transformed_build.has_value()) {
                continue;
            }
            const Build& build         = *package.transformed_build;
            const bool use_user_values = (package.database_config->type != PackageType::Compiler &&
                                          package.database_config->type != PackageType::Mpi) ||
                                         yaml_has(package.user_config, "build");
            if (build.configurations.has_value()) {
                compute_configuration(*build.configurations, package.build_configuration_index,
                                      *package.database_config, context, use_user_values,
                                      apply_conditions);
            }
            for (std::size_t stage_index = 0; stage_index < build.stages.size(); ++stage_index) {
                const BuildStage& stage = build.stages[stage_index];
                if (stage.configurations.has_value()) {
                    if (stage_index >= package.stage_configuration_indices.size()) {
                        user_config_error("internal user stage index is missing for package '" +
                                          package.database_config->name + "'");
                    }
                    compute_configuration(
                        *stage.configurations, package.stage_configuration_indices[stage_index],
                        *package.database_config, context, use_user_values, apply_conditions);
                }
            }
        }
        return context.config_version != previous_version;
    }

}  // namespace

void precompute_parser_values(UserConfigParserContext& context) {
    compute_values(context, false);

    const std::size_t max_passes =
        context.named_option_values.size() + context.named_environment_values.size() + 1;
    for (std::size_t pass = 0; pass < max_passes; ++pass) {
        if (!compute_values(context, true)) {
            return;
        }
    }
    user_config_error("conditional configuration values did not converge");
}
