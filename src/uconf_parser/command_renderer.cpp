/**
 * @file command_renderer.cpp
 * @brief Rendering of resolved package build configurations into shell commands.
 */

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <uconf_parser/parser_internal.hpp>
#include <utility>
#include <utils/bash_utils.hpp>
#include <utils/string_utils.hpp>
#include <utils/yaml_utils.hpp>
#include <vector>

namespace {

    std::string option_name(const std::string& name, Toolchain toolchain) {
        if (toolchain == Toolchain::Autotools) {
            if (name.rfind("--", 0) == 0 || name.rfind('-', 0) == 0 || is_shell_assignment(name)) {
                return name;
            }
            return "--" + name;
        }
        if (toolchain == Toolchain::CMake || toolchain == Toolchain::Meson) {
            return name.rfind('-', 0) == 0 ? name : "-D" + name;
        }
        return name;
    }

    std::string render_option(const BuildOption& option, const ParsedOptionState& state,
                              Toolchain toolchain, UserConfigParserContext& context) {
        const std::string format = state.enabled ? option.enabled_format.value_or(option.name)
                                                 : option.disabled_format.value_or("");
        if (format.empty()) {
            return {};
        }

        std::string result      = option_name(format, toolchain);
        const std::string value = resolve_parser_scalar(
            state.enabled ? state.enabled_value : state.disabled_value, context);
        if (!value.empty()) {
            result += "=" + shell_double_quote(value);
        }
        return result;
    }

    std::string render_configuration_options(const BuildConfiguration& configuration,
                                             Toolchain toolchain,
                                             UserConfigParserContext& context) {
        std::vector<std::string> options;
        for (const BuildOption& option : configuration.options) {
            const auto parsed = context.option_values.find(&option);
            if (parsed == context.option_values.end()) {
                user_config_error("internal option state is missing for '" + option.name + "'");
            }
            const std::string rendered = render_option(option, parsed->second, toolchain, context);
            if (!rendered.empty()) {
                options.push_back(rendered);
            }
        }
        return join(options);
    }

    void append_configuration_commands(const BuildConfiguration& configuration, Toolchain toolchain,
                                       const std::string& command, UserConfigParserContext& context,
                                       std::vector<std::string>& commands) {
        std::string resolved_command = resolve_parser_scalar(command, context);
        const std::string options = render_configuration_options(configuration, toolchain, context);
        if (!options.empty()) {
            resolved_command += (resolved_command.empty() ? "" : " ") + options;
        }
        if (resolved_command.empty()) {
            return;
        }

        std::vector<std::pair<std::string, std::optional<std::string>>> previous_values;
        for (const EnvironmentVariable& variable : configuration.environment) {
            const auto parsed = context.environment_values.find(&variable);
            if (parsed == context.environment_values.end() || parsed->second.empty()) {
                continue;
            }
            const char* previous = std::getenv(variable.name.c_str());
            previous_values.emplace_back(variable.name, previous == nullptr
                                                            ? std::nullopt
                                                            : std::optional<std::string>(previous));
            commands.push_back("export " + variable.name + "=" +
                               shell_double_quote(resolve_parser_scalar(parsed->second, context)));
        }
        commands.push_back(resolved_command);
        for (const auto& [name, previous] : previous_values) {
            if (!previous.has_value()) {
                commands.push_back("unset " + name);
            } else {
                commands.push_back("export " + name + "=" + shell_single_quote(*previous));
            }
        }
    }

    void append_patch_commands(const ParsedUserPackage& package, UserConfigParserContext& context,
                               std::vector<std::string>& commands) {
        if (!yaml_has(package.user_config, "patches")) {
            return;
        }
        const YAML::Node patches = package.user_config["patches"];
        if (!patches.IsSequence()) {
            user_config_error("package patches must be a sequence");
        }
        for (const YAML::Node& patch : patches) {
            if (!patch.IsMap() || !yaml_has(patch, "name") || !yaml_has(patch, "enabled")) {
                user_config_error("patch entries must contain name and enabled fields");
            }
            if (!yaml_boolean(patch["enabled"], "patch enabled value")) {
                continue;
            }
            const std::string name = yaml_scalar(patch["name"], "patch name");
            if (std::filesystem::path(name).filename() != name) {
                user_config_error("invalid patch name '" + name + "'");
            }
            const std::filesystem::path path =
                context.settings.kez_home / "patches" / package.requested_name / name;
            if (!std::filesystem::is_regular_file(path)) {
                user_config_error("patch file does not exist: " + path.string());
            }
            commands.push_back("git apply " + shell_single_quote(path.string()));
        }
    }

}  // namespace

std::vector<std::string> generate_package_commands(const ParsedUserPackage& package,
                                                   UserConfigParserContext& context) {
    std::vector<std::string> commands;
    context.current_package = package.requested_name;
    if (package.database_config->toolchain() == Toolchain::Python) {
        const std::filesystem::path prefix = parser_package_prefix(package.requested_name, context);
        commands.push_back("mkdir -p " + shell_single_quote(prefix.string()));
        return commands;
    }
    if (!package.transformed_build.has_value()) {
        return commands;
    }
    if ((package.database_config->type == PackageType::Compiler ||
         package.database_config->type == PackageType::Mpi) &&
        !yaml_has(package.user_config, "build")) {
        return commands;
    }
    if (package.database_config->type == PackageType::Vendor) {
        const std::filesystem::path prefix = parser_package_prefix(package.requested_name, context);
        if (std::filesystem::exists(prefix)) {
            return commands;
        }
        commands.push_back("mkdir -p " + shell_single_quote(prefix.string()));
    }

    append_source_commands(package, context, commands);
    append_patch_commands(package, context, commands);

    const Build& build = *package.transformed_build;
    if (build.preprocessing.has_value()) {
        commands.push_back(resolve_parser_scalar(*build.preprocessing, context));
    }
    if (build.configurations.has_value()) {
        const std::optional<std::string> default_command =
            package.database_config->default_configuration_command();
        if (package.database_config->toolchain() != Toolchain::None ||
            build.configurations->command.has_value()) {
            append_configuration_commands(
                *build.configurations, package.database_config->toolchain(),
                build.configurations->command.value_or(default_command.value_or("")), context,
                commands);
        }
    }
    for (const BuildStage& stage : build.stages) {
        BuildStage resolved_stage = stage;
        if (resolved_stage.target.has_value()) {
            resolved_stage.target = resolve_parser_scalar(*resolved_stage.target, context);
        }
        const std::optional<std::string> default_command =
            package.database_config->default_stage_command(resolved_stage,
                                                           context.settings.parallel_jobs);
        const std::string command =
            stage.configurations.has_value() && stage.configurations->command.has_value()
                ? *stage.configurations->command
                : default_command.value_or("");
        if (stage.configurations.has_value()) {
            if (package.database_config->toolchain() != Toolchain::None ||
                stage.configurations->command.has_value()) {
                append_configuration_commands(*stage.configurations, Toolchain::None, command,
                                              context, commands);
            }
        } else if (!command.empty()) {
            commands.push_back(resolve_parser_scalar(command, context));
        }
    }
    if (build.postprocessing.has_value()) {
        commands.push_back(resolve_parser_scalar(*build.postprocessing, context));
    }
    return commands;
}
