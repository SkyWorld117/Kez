#include <algorithm>
#include <database/database.hpp>
#include <database/resolve_utils.hpp>
#include <dependency_resolver/requirements.hpp>
#include <string>
#include <unordered_set>
#include <user_config_generator/config_transformer.hpp>
#include <utility>
#include <utils/string_utils.hpp>
#include <vector>

namespace {

    /**
     * @brief Derive a canonical, toolchain-agnostic option key from an option name.
     *
     * Strips well-known toolchain-specific prefixes so that different but
     * semantically equivalent options (e.g. "--prefix" for Autotools and
     * "-DCMAKE_INSTALL_PREFIX" for CMake) can be compared or deduplicated by
     * their core key.
     *
     * The stripping rules depend on @p toolchain:
     *   - Autotools: removes a leading "--" prefix.
     *   - CMake:     removes a leading "-D" prefix.
     *
     * After the above prefix removal, if the toolchain is Autotools and the
     * name does not look like a shell assignment (e.g. "CC=..."), any leading
     * dashes are also stripped so that "-jN" normalises to "jN".
     *
     * Finally, if the name contains an '=' sign, everything from the first '='
     * onward is removed, yielding only the left-hand-side key.
     *
     * @param name      The raw option name (e.g. "--prefix=/usr",
     *                  "-DCMAKE_BUILD_TYPE=Release").
     * @param toolchain The build toolchain in use, which determines the prefix
     *                  to strip.
     * @return The canonical key string (e.g. "prefix", "CMAKE_BUILD_TYPE").
     *
     * @note Does not terminate the program; returns an empty string only if
     *       @p name consists solely of stripped characters.
     */
    std::string option_key(std::string name, Toolchain toolchain) {
        if (toolchain == Toolchain::Autotools && name.rfind("--", 0) == 0) {
            name.erase(0, 2);
        } else if (toolchain == Toolchain::CMake && name.rfind("-D", 0) == 0) {
            name.erase(0, 2);
        }
        if (toolchain == Toolchain::Autotools && !is_shell_assignment(name) &&
            name.rfind('-', 0) == 0) {
            name.erase(0, name.find_first_not_of('-'));
        }
        const std::size_t value = name.find('=');
        return value == std::string::npos ? name : name.substr(0, value);
    }

    std::string template_value(const std::string& package, const std::string& property) {
        return "${" + package + "." + property + "}";
    }

    bool selected(const std::string& package, const std::unordered_set<std::string>& dependencies,
                  const AbstractPackageSelections& abstract_packages) {
        return requirements_satisfied({package}, dependencies, abstract_packages);
    }

    std::vector<std::string> active_dependencies(
        const BuildConfiguration& configuration, const PackageConfig& package,
        const std::unordered_set<std::string>& dependencies,
        const AbstractPackageSelections& abstract_packages) {
        std::vector<std::string> result;
        for (const std::string& dependency : package.dependencies) {
            if (selected(dependency, dependencies, abstract_packages)) {
                append_unique(result, dependency);
            }
        }

        // Helper lambda: if the full requirement list is satisfied, append
        // each package named in it to the result.
        auto append_requirements = [&](const std::vector<std::string>& requirements) {
            if (!requirements_satisfied(requirements, dependencies, abstract_packages)) {
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
            append_requirements(option.requires);
        }
        return result;
    }

    struct DependencyDefaults {
        std::vector<std::string> include_flags;
        std::vector<std::string> linker_flags;
        std::vector<std::string> libraries;
        std::vector<std::string> library_paths;
        std::vector<std::string> prefix_paths;
    };

    DependencyDefaults dependency_defaults(const BuildConfiguration& configuration,
                                           const PackageConfig& package,
                                           const std::unordered_set<std::string>& dependencies,
                                           const AbstractPackageSelections& abstract_packages,
                                           const std::string& compiler) {
        DependencyDefaults result;

        if (package_has_property("compiler", "ldflags", abstract_packages, compiler)) {
            append_unique(result.linker_flags, template_value("compiler", "ldflags"));
        }
        if (package_has_property("compiler", "lib", abstract_packages, compiler)) {
            append_unique(result.library_paths, template_value("compiler", "lib"));
        }

        for (const std::string& dependency :
             active_dependencies(configuration, package, dependencies, abstract_packages)) {
            if (package_has_property(dependency, "includes", abstract_packages, compiler)) {
                append_unique(result.include_flags, template_value(dependency, "includes"));
            }
            if (package_has_property(dependency, "ldflags", abstract_packages, compiler)) {
                append_unique(result.linker_flags, template_value(dependency, "ldflags"));
            }
            if (package_has_property(dependency, "lib", abstract_packages, compiler)) {
                append_unique(result.library_paths, template_value(dependency, "lib"));
            }
            if (package_has_property(dependency, "libs", abstract_packages, compiler)) {
                append_unique(result.libraries, template_value(dependency, "libs"));
            }
            // Prefix is a built-in property; every package has a resolvable
            // installation prefix regardless of whether it is explicitly declared
            // in the package's properties list.
            result.prefix_paths.push_back(template_value(dependency, "prefix"));
        }

        // The package itself should be added to the library paths
        if (package_has_property(package.name, "ldflags", abstract_packages, compiler)) {
            append_unique(result.linker_flags, template_value(package.name, "ldflags"));
        }
        if (package_has_property(package.name, "lib", abstract_packages, compiler)) {
            append_unique(result.library_paths, template_value(package.name, "lib"));
        }

        return result;
    }

    std::string compiler_property(const std::string& property,
                                  const std::vector<std::string>& dependencies,
                                  const AbstractPackageSelections& abstract_packages,
                                  const std::string& compiler) {
        if (std::find(dependencies.begin(), dependencies.end(), "mpi") != dependencies.end() &&
            package_has_property("mpi", property, abstract_packages, compiler)) {
            return template_value("mpi", property);
        }
        if (package_has_property("compiler", property, abstract_packages, compiler)) {
            return template_value("compiler", property);
        }
        return {};
    }

    std::unordered_set<std::string> explicit_option_keys(const BuildConfiguration& configuration,
                                                         Toolchain toolchain) {
        std::unordered_set<std::string> result;
        for (const BuildOption& option : configuration.options) {
            result.emplace(option_key(option.name, toolchain));
            if (option.enabled_format.has_value()) {
                result.emplace(option_key(*option.enabled_format, toolchain));
            }
            if (option.disabled_format.has_value()) {
                result.emplace(option_key(*option.disabled_format, toolchain));
            }
        }
        return result;
    }

    void append_default(BuildConfiguration& configuration,
                        const std::unordered_set<std::string>& explicit_options,
                        Toolchain toolchain, const std::string& name, const std::string& value) {
        if (value.empty() ||
            explicit_options.find(option_key(name, toolchain)) != explicit_options.end()) {
            return;
        }

        BuildOption option;
        option.name                         = name;
        option.user_configurable            = true;
        option.enabled_value                = ConfigurableValue<std::string> {};
        option.enabled_value->default_value = value;
        configuration.options.push_back(std::move(option));
    }

}  // namespace

namespace user_config_generator {

    BuildConfiguration transformed_configuration(
        const BuildConfiguration& configuration, const PackageConfig& package, Toolchain toolchain,
        const std::unordered_set<std::string>& dependencies,
        const AbstractPackageSelections& abstract_packages, const std::string& compiler) {
        BuildConfiguration result = configuration;
        if (toolchain != Toolchain::Autotools && toolchain != Toolchain::CMake) {
            return result;
        }

        const std::unordered_set<std::string> explicit_options =
            explicit_option_keys(configuration, toolchain);
        const DependencyDefaults paths =
            dependency_defaults(configuration, package, dependencies, abstract_packages, compiler);
        const std::vector<std::string> dependency_names =
            active_dependencies(configuration, package, dependencies, abstract_packages);

        const std::string include_flags = join(paths.include_flags);
        const std::string linker_flags  = join(paths.linker_flags);
        const std::string libraries     = join(paths.libraries);
        const std::string all_linker_flags =
            linker_flags + (linker_flags.empty() || libraries.empty() ? "" : " ") + libraries;
        const std::string compiler_flags = include_flags.empty() ? "-O3" : "-O3 " + include_flags;

        if (toolchain == Toolchain::Autotools) {
            append_default(result, explicit_options, toolchain, "prefix",
                           template_value(package.name, "prefix"));
            append_default(result, explicit_options, toolchain, "CC",
                           compiler_property("c", dependency_names, abstract_packages, compiler));
            append_default(result, explicit_options, toolchain, "CXX",
                           compiler_property("cxx", dependency_names, abstract_packages, compiler));
            append_default(
                result, explicit_options, toolchain, "FC",
                compiler_property("fort", dependency_names, abstract_packages, compiler));
            append_default(result, explicit_options, toolchain, "CPPFLAGS", include_flags);
            append_default(result, explicit_options, toolchain, "CFLAGS", compiler_flags);
            append_default(result, explicit_options, toolchain, "CXXFLAGS", compiler_flags);
            append_default(result, explicit_options, toolchain, "FCFLAGS", compiler_flags);
            append_default(result, explicit_options, toolchain, "LDFLAGS", linker_flags);
        } else {
            append_default(result, explicit_options, toolchain, "CMAKE_INSTALL_PREFIX",
                           template_value(package.name, "prefix"));
            append_default(result, explicit_options, toolchain, "CMAKE_BUILD_TYPE", "Release");
            append_default(result, explicit_options, toolchain, "CMAKE_C_COMPILER",
                           compiler_property("c", dependency_names, abstract_packages, compiler));
            append_default(result, explicit_options, toolchain, "CMAKE_CXX_COMPILER",
                           compiler_property("cxx", dependency_names, abstract_packages, compiler));
            append_default(
                result, explicit_options, toolchain, "CMAKE_Fortran_COMPILER",
                compiler_property("fort", dependency_names, abstract_packages, compiler));
            append_default(result, explicit_options, toolchain, "CMAKE_C_FLAGS", compiler_flags);
            append_default(result, explicit_options, toolchain, "CMAKE_CXX_FLAGS", compiler_flags);
            append_default(result, explicit_options, toolchain, "CMAKE_Fortran_FLAGS",
                           compiler_flags);
            append_default(result, explicit_options, toolchain, "CMAKE_CUDA_FLAGS", compiler_flags);
            append_default(result, explicit_options, toolchain, "CMAKE_EXE_LINKER_FLAGS",
                           all_linker_flags);
            append_default(result, explicit_options, toolchain, "CMAKE_SHARED_LINKER_FLAGS",
                           all_linker_flags);
            append_default(result, explicit_options, toolchain, "CMAKE_MODULE_LINKER_FLAGS",
                           all_linker_flags);
            append_default(result, explicit_options, toolchain, "CMAKE_BUILD_RPATH",
                           join(paths.library_paths, ";"));
            append_default(result, explicit_options, toolchain, "CMAKE_INSTALL_RPATH",
                           join(paths.library_paths, ";"));
            append_default(result, explicit_options, toolchain, "CMAKE_PREFIX_PATH",
                           join(paths.prefix_paths, ";"));
        }

        return result;
    }

    std::optional<Build> transformed_build(const PackageConfig& package,
                                           const std::unordered_set<std::string>& dependencies,
                                           const AbstractPackageSelections& abstract_packages,
                                           const std::string& compiler) {
        if (!package.build.has_value()) {
            return std::nullopt;
        }

        Build result = *package.build;
        if (result.configurations.has_value()) {
            result.configurations =
                transformed_configuration(*result.configurations, package, package.toolchain(),
                                          dependencies, abstract_packages, compiler);
        }
        return result;
    }

}  // namespace user_config_generator
