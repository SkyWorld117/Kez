#include <algorithm>
#include <cctype>
#include <dependency_resolver/requirements.hpp>
#include <optional>
#include <parser/config_transformer.hpp>
#include <parser/parser_internal.hpp>
#include <string>
#include <unordered_set>
#include <utils/bash_utils.hpp>
#include <utils/yaml_utils.hpp>
#include <vector>

namespace {

    bool is_shell_assignment(const std::string& name) {
        if (name.empty() ||
            (name[0] != '_' && !std::isupper(static_cast<unsigned char>(name[0])))) {
            return false;
        }
        return std::all_of(name.begin() + 1, name.end(), [](const char character) {
            return character == '_' || std::isupper(static_cast<unsigned char>(character)) ||
                   std::isdigit(static_cast<unsigned char>(character));
        });
    }

    std::string option_name(const std::string& name, Toolchain toolchain) {
        if (toolchain == Toolchain::Autotools) {
            if (name.rfind("--", 0) == 0 || name.rfind('-', 0) == 0 || is_shell_assignment(name)) {
                return name;
            }
            return "--" + name;
        }
        if (toolchain == Toolchain::CMake) {
            return name.rfind('-', 0) == 0 ? name : "-D" + name;
        }
        return name;
    }

    std::string option_key(std::string name, Toolchain toolchain) {
        if (toolchain == Toolchain::Autotools && name.rfind("--", 0) == 0) {
            name.erase(0, 2);
        } else if (toolchain == Toolchain::CMake && name.rfind("-D", 0) == 0) {
            name.erase(0, 2);
        }
        const std::size_t value = name.find('=');
        return value == std::string::npos ? name : name.substr(0, value);
    }

    void append_unique(std::vector<std::string>& values, const std::string& value) {
        if (!value.empty() && std::find(values.begin(), values.end(), value) == values.end()) {
            values.push_back(value);
        }
    }

    std::string join(const std::vector<std::string>& values, const std::string& separator = " ") {
        std::string result;
        for (const std::string& value : values) {
            result += (result.empty() ? "" : separator) + value;
        }
        return result;
    }

    std::string selected_compiler(UserConfigParserContext& context) {
        const auto package = context.package_indices.find(context.current_package);
        if (package == context.package_indices.end()) {
            return "gcc";
        }
        const YAML::Node user_package = context.packages[package->second].user_config;
        if (!yaml_has(user_package, "compiler")) {
            return "gcc";
        }
        const std::string specification = yaml_scalar(user_package["compiler"], "package compiler");
        if (specification == "system") {
            return "gcc";
        }
        const std::size_t separator = specification.find('@');
        return separator == std::string::npos ? specification : specification.substr(0, separator);
    }

    std::optional<std::string> package_property(const std::string& package,
                                                const std::string& property,
                                                UserConfigParserContext& context) {
        if (!parser_package_has_property(package, property, context)) {
            return std::nullopt;
        }
        return resolve_parser_scalar("${" + package + "." + property + "}", context);
    }

    std::vector<std::string> active_dependencies(const BuildConfiguration& configuration,
                                                 const PackageConfig& package,
                                                 UserConfigParserContext& context) {
        std::vector<std::string> result = package.dependencies;
        auto append_requirements        = [&](const std::vector<std::string>& requirements) {
            if (!requirements_satisfied(requirements, context.dependencies,
                                        context.abstract_packages)) {
                return;
            }
            for (const std::string& requirement : requirements) {
                append_unique(result, requirement);
            }
        };

        for (const EnvironmentVariable& variable : configuration.environment) {
            append_requirements(variable.requires);
        }
        for (const BuildOption& option : configuration.options) {
            const auto state = context.option_values.find(&option);
            if (state != context.option_values.end() && state->second.enabled) {
                append_requirements(option.requires);
            }
        }
        return result;
    }

    struct DependencyPaths {
        std::vector<std::string> include_flags;
        std::vector<std::string> linker_flags;
        std::vector<std::string> libraries;
        std::vector<std::string> library_paths;
        std::vector<std::string> prefixes;
    };

    DependencyPaths dependency_paths(const BuildConfiguration& configuration,
                                     const PackageConfig& package,
                                     UserConfigParserContext& context) {
        DependencyPaths result;
        append_unique(result.prefixes, context.settings.install_prefix.string());

        if (const auto compiler_flags = package_property("compiler", "ldflags", context)) {
            append_unique(result.linker_flags, *compiler_flags);
        }
        if (const auto compiler_lib = package_property("compiler", "lib", context)) {
            append_unique(result.library_paths, *compiler_lib);
        }

        for (const std::string& dependency : active_dependencies(configuration, package, context)) {
            append_unique(result.prefixes, parser_package_prefix(dependency, context));

            if (const auto includes = package_property(dependency, "includes", context)) {
                append_unique(result.include_flags, *includes);
            }

            if (const auto flags = package_property(dependency, "ldflags", context)) {
                append_unique(result.linker_flags, *flags);
            }
            if (const auto lib = package_property(dependency, "lib", context)) {
                append_unique(result.library_paths, *lib);
            }

            if (const auto libraries = package_property(dependency, "libs", context)) {
                append_unique(result.libraries, *libraries);
            }
        }
        return result;
    }

    std::string compiler_property(const std::string& property,
                                  const std::vector<std::string>& dependencies,
                                  UserConfigParserContext& context) {
        if (std::find(dependencies.begin(), dependencies.end(), "mpi") != dependencies.end() &&
            parser_package_has_property("mpi", property, context)) {
            return resolve_parser_scalar("${mpi." + property + "}", context);
        }
        if (parser_package_has_property("compiler", property, context)) {
            return resolve_parser_scalar("${compiler." + property + "}", context);
        }
        return {};
    }

    std::string render_option(const std::string& format, const std::string& value,
                              Toolchain toolchain) {
        std::string result = option_name(format, toolchain);
        if (!value.empty()) {
            result += "=" + shell_double_quote(value);
        }
        return result;
    }

}  // namespace

namespace parser {

    std::string format_include_path(const std::string& path) { return "-I" + path; }

    std::string format_nvidia_library_path(const std::string& path) {
        return "-L" + path + " -Xlinker -rpath," + path;
    }

    std::string format_library_path(const std::string& path, UserConfigParserContext& context) {
        const std::string compiler = selected_compiler(context);
        if (compiler.find("nvhpc") != std::string::npos || compiler == "nvc" ||
            compiler == "nvcc") {
            return format_nvidia_library_path(path);
        }
        return "-L" + path + " -Wl,-rpath," + path;
    }

    std::string transform_configuration(const BuildConfiguration& configuration,
                                        const PackageConfig& package, Toolchain toolchain,
                                        UserConfigParserContext& context) {
        std::vector<std::string> options;
        std::unordered_set<std::string> explicit_options;

        for (const BuildOption& option : configuration.options) {
            const auto parsed = context.option_values.find(&option);
            if (parsed == context.option_values.end()) {
                user_config_error("internal option state is missing for '" + option.name + "'");
            }
            const ParsedOptionState& state = parsed->second;
            const std::string format = state.enabled ? option.enabled_format.value_or(option.name)
                                                     : option.disabled_format.value_or("");
            if (format.empty()) {
                explicit_options.emplace(option_key(option.name, toolchain));
                continue;
            }
            explicit_options.emplace(option_key(format, toolchain));
            const std::string& value = state.enabled ? state.enabled_value : state.disabled_value;
            options.push_back(
                render_option(format, resolve_parser_scalar(value, context), toolchain));
        }

        if (toolchain != Toolchain::Autotools && toolchain != Toolchain::CMake) {
            return join(options);
        }

        const DependencyPaths paths = dependency_paths(configuration, package, context);
        const std::vector<std::string> dependencies =
            active_dependencies(configuration, package, context);
        const std::string include_flags = join(paths.include_flags);
        const std::string linker_flags  = join(paths.linker_flags);
        const std::string libraries     = join(paths.libraries);
        const std::string all_linker_flags =
            linker_flags + (linker_flags.empty() || libraries.empty() ? "" : " ") + libraries;

        std::string opt_flags = "-O3";
        if (!include_flags.empty()) {
            opt_flags += " ";
        }

        auto append_default = [&](const std::string& name, const std::string& value) {
            if (value.empty() ||
                explicit_options.find(option_key(name, toolchain)) != explicit_options.end()) {
                return;
            }
            options.push_back(render_option(name, value, toolchain));
        };

        if (toolchain == Toolchain::Autotools) {
            append_default("prefix", parser_package_prefix(context.current_package, context));
            append_default("CC", compiler_property("c", dependencies, context));
            append_default("CXX", compiler_property("cxx", dependencies, context));
            append_default("FC", compiler_property("fort", dependencies, context));
            append_default("CPPFLAGS", include_flags);
            append_default("CFLAGS", opt_flags + include_flags);
            append_default("CXXFLAGS", opt_flags + include_flags);
            append_default("FCFLAGS", opt_flags + include_flags);
            append_default("LDFLAGS", linker_flags);
        } else if (toolchain == Toolchain::CMake) {
            append_default("CMAKE_INSTALL_PREFIX",
                           parser_package_prefix(context.current_package, context));
            append_default("CMAKE_PREFIX_PATH", join(paths.prefixes, ";"));
            append_default("CMAKE_BUILD_TYPE", "Release");
            append_default("CMAKE_C_COMPILER", compiler_property("c", dependencies, context));
            append_default("CMAKE_CXX_COMPILER", compiler_property("cxx", dependencies, context));
            append_default("CMAKE_Fortran_COMPILER",
                           compiler_property("fort", dependencies, context));
            append_default("CMAKE_C_FLAGS", opt_flags + include_flags);
            append_default("CMAKE_CXX_FLAGS", opt_flags + include_flags);
            append_default("CMAKE_Fortran_FLAGS", opt_flags + include_flags);
            append_default("CMAKE_CUDA_FLAGS", opt_flags + include_flags);
            append_default("CMAKE_EXE_LINKER_FLAGS", all_linker_flags);
            append_default("CMAKE_SHARED_LINKER_FLAGS", all_linker_flags);
            append_default("CMAKE_MODULE_LINKER_FLAGS", all_linker_flags);
            append_default("CMAKE_BUILD_RPATH", join(paths.library_paths, ";"));
            append_default("CMAKE_INSTALL_RPATH", join(paths.library_paths, ";"));
        }

        return join(options);
    }

}  // namespace parser
