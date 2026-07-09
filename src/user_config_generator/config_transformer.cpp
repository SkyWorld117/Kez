#include <algorithm>
#include <cctype>
#include <database/database.hpp>
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

    /**
     * @brief Parse a compiler specification into a name-and-version pair.
     *
     * Accepts strings of the form:
     *   - "gcc@12"  -> {"gcc", "12"}
     *   - "gcc"     -> {"gcc", "latest"}
     *   - ""        -> {"gcc", "latest"}  (empty string treated as system)
     *   - "system"  -> {"gcc", "latest"}  (system compiler falls back to gcc)
     *
     * The separator '@' splits the name (left) from the version (right).
     * If no '@' is present, the version defaults to "latest".
     *
     * @param compiler The raw compiler string (e.g. "gcc@12", "llvm", "system").
     * @return A pair where `first` is the compiler name and `second` is the
     *         version string.
     *
     * @note Does not terminate the program. Returns {gcc, latest} for any
     *       empty or unrecognised sentinel value.
     */
    std::pair<std::string, std::string> parse_compiler(const std::string& compiler) {
        if (compiler.empty() || compiler == "system") {
            return {"gcc", "latest"};
        }
        const std::size_t separator = compiler.find('@');
        if (separator == std::string::npos) {
            return {compiler, "latest"};
        }
        return {compiler.substr(0, separator), compiler.substr(separator + 1)};
    }

    /**
     * @brief Retrieve the PackageConfig for the package that provides a given
     *        named property.
     *
     * Properties are normally looked up on the package itself. The special
     * package name "compiler" causes the lookup to be redirected to the
     * compiler's own recipe (via parse_compiler). For any other package,
     * the function first checks @p abstract_packages: if the package is an
     * abstract name (e.g. "BLAS"), the concrete implementation is used
     * instead; otherwise the name is used as-is.
     *
     * @param package           The package whose config should be fetched.
     *                          "compiler" is a reserved sentinel that redirects
     *                          to the compiler's recipe.
     * @param abstract_packages Mapping from abstract package names to selected
     *                          concrete implementations.
     * @param compiler          Raw compiler string; only used when @p package
     *                          is "compiler" (passed to parse_compiler).
     * @return A shared pointer to the resolved PackageConfig.
     *
     * @warning Terminates the program (via ERROR macro inside get_db_config())
     *          if the looked-up recipe does not exist in the database.
     */
    PackageConfigPtr property_config(const std::string& package,
                                     const AbstractPackageSelections& abstract_packages,
                                     const std::string& compiler) {
        if (package == "compiler") {
            const auto [compiler_name, compiler_version] = parse_compiler(compiler);
            return get_db_config(compiler_name, compiler_version);
        }
        const auto abstract = abstract_packages.find(package);
        return get_db_config(abstract == abstract_packages.end() ? package : abstract->second);
    }

    /**
     * @brief Check whether a given package declares a named property that can
     *        be referenced as a template variable.
     *
     * Resolves the package's config via property_config() (which handles
     * abstract-to-concrete mapping and the "compiler" sentinel), then queries
     * has_property() on that config.
     *
     * @param package           The package name (or sentinel "compiler") to
     *                          inspect.
     * @param property          The property name to check for (e.g.
     *                          "includes", "ldflags", "lib", "libs").
     * @param abstract_packages Mapping from abstract package names to concrete
     *                          implementations.
     * @param compiler          Raw compiler string; forwarded to
     *                          property_config() when @p package is "compiler".
     * @return true if the resolved package config declares the property (or
     *         one of its recognised aliases); false otherwise.
     *
     * @warning May terminate the program via property_config() /
     *          get_db_config() if the package recipe cannot be found.
     */
    bool has_template_property(const std::string& package, const std::string& property,
                               const AbstractPackageSelections& abstract_packages,
                               const std::string& compiler) {
        return has_property(*property_config(package, abstract_packages, compiler), property);
    }

    /**
     * @brief Build a template-variable reference string for a package property.
     *
     * Produces a string of the form "${package.property}" that the template
     * resolver later substitutes with the actual property value at generation
     * time.
     *
     * @param package  The package name that owns the property.
     * @param property The property name (e.g. "prefix", "includes", "lib").
     * @return A template string "${package.property}".
     */
    std::string template_value(const std::string& package, const std::string& property) {
        return "${" + package + "." + property + "}";
    }

    /**
     * @brief Determine whether a package is selected in the resolved dependency
     *        set.
     *
     * A package is considered "selected" if its name (or, if it is an abstract
     * package, its concrete mapping) appears in the dependency set.  Delegates
     * to requirements_satisfied() with a single-element requirement list.
     *
     * @param package           The package name to test.
     * @param dependencies      The set of concrete package names resolved for
     *                          the current build.
     * @param abstract_packages Mapping from abstract package names to concrete
     *                          implementations used to resolve @p package if it
     *                          is abstract.
     * @return true if @p package (or its concrete implementation) is in the
     *         dependency set; false otherwise.
     */
    bool selected(const std::string& package, const std::unordered_set<std::string>& dependencies,
                  const AbstractPackageSelections& abstract_packages) {
        return requirements_satisfied({package}, dependencies, abstract_packages);
    }

    /**
     * @brief Compute the list of dependencies that are both declared by a
     *        package and selected (active) in the current resolution context.
     *
     * Iterates over the package's declared dependencies and retains only those
     * that appear in the resolved dependency set (see selected()).  Beyond the
     * package's own dependency list, the function also inspects the build
     * configuration's environment variables and build options: any packages
     * listed in their `requires` fields are added to the active set if their
     * requirements are satisfied.
     *
     * Duplicates are suppressed via append_unique().
     *
     * @param configuration     The build configuration whose environment
     *                          variables and options may carry additional
     *                          requirements.
     * @param package           The PackageConfig whose dependencies list is
     *                          scanned.
     * @param dependencies      The resolved concrete dependency set for the
     *                          current build.
     * @param abstract_packages Mapping from abstract package names to concrete
     *                          implementations.
     * @return A vector of concrete package names that are active in the
     *         current build context.
     */
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

    /**
     * @brief Aggregated default compiler and linker flags derived from a
     *        package's active dependencies.
     *
     * This struct collects the four categories of flags that a typical build
     * toolchain (Autotools or CMake) needs:
     *   - include_flags:  -I include directories
     *   - linker_flags:   -L / -Wl, linker directives
     *   - libraries:      -l library names
     *   - library_paths:  rpath / lib search directories
     */
    struct DependencyDefaults {
        std::vector<std::string> include_flags;
        std::vector<std::string> linker_flags;
        std::vector<std::string> libraries;
        std::vector<std::string> library_paths;
    };

    /**
     * @brief Collect default compiler and linker flags from the compiler and
     *        all active dependencies.
     *
     * The function processes three sources in order:
     *   1. The compiler itself: if it exposes "ldflags" and "lib" properties,
     *      these are added as template-variable references.
     *   2. Each active dependency (see active_dependencies()): if it exposes
     *      "includes", "ldflags", "lib", and/or "libs" properties, those are
     *      added as template-variable references.
     *   3. The package itself: its own "ldflags" and "lib" properties are
     *      appended (the package's own library paths must be available to
     *      dependent packages).
     *
     * All entries are added via append_unique() so that duplicate references
     * to the same property from different dependency paths are suppressed.
     *
     * @param configuration     The build configuration (used to compute active
     *                          dependencies).
     * @param package           The PackageConfig whose own properties may
     *                          contribute to the defaults.
     * @param dependencies      The resolved concrete dependency set.
     * @param abstract_packages Mapping from abstract to concrete package names.
     * @param compiler          Raw compiler string forwarded to
     *                          has_template_property() for the "compiler"
     *                          sentinel lookup.
     * @return A DependencyDefaults struct containing the aggregated template
     *         variable references.
     *
     * @warning Terminates the program via has_template_property() /
     *          get_db_config() if any needed package recipe is missing from the
     *          database.
     */
    DependencyDefaults dependency_defaults(const BuildConfiguration& configuration,
                                           const PackageConfig& package,
                                           const std::unordered_set<std::string>& dependencies,
                                           const AbstractPackageSelections& abstract_packages,
                                           const std::string& compiler) {
        DependencyDefaults result;

        if (has_template_property("compiler", "ldflags", abstract_packages, compiler)) {
            append_unique(result.linker_flags, template_value("compiler", "ldflags"));
        }
        if (has_template_property("compiler", "lib", abstract_packages, compiler)) {
            append_unique(result.library_paths, template_value("compiler", "lib"));
        }

        for (const std::string& dependency :
             active_dependencies(configuration, package, dependencies, abstract_packages)) {
            if (has_template_property(dependency, "includes", abstract_packages, compiler)) {
                append_unique(result.include_flags, template_value(dependency, "includes"));
            }
            if (has_template_property(dependency, "ldflags", abstract_packages, compiler)) {
                append_unique(result.linker_flags, template_value(dependency, "ldflags"));
            }
            if (has_template_property(dependency, "lib", abstract_packages, compiler)) {
                append_unique(result.library_paths, template_value(dependency, "lib"));
            }
            if (has_template_property(dependency, "libs", abstract_packages, compiler)) {
                append_unique(result.libraries, template_value(dependency, "libs"));
            }
        }

        // The package itself should be added to the library paths
        if (has_template_property(package.name, "ldflags", abstract_packages, compiler)) {
            append_unique(result.linker_flags, template_value(package.name, "ldflags"));
        }
        if (has_template_property(package.name, "lib", abstract_packages, compiler)) {
            append_unique(result.library_paths, template_value(package.name, "lib"));
        }

        return result;
    }

    /**
     * @brief Obtain a template-variable reference for a compiler (or MPI)
     *        property such as the C, CXX, or Fortran compiler name.
     *
     * The lookup prefers the MPI implementation if MPI is among the active
     * dependencies and it exposes the requested property; otherwise falls
     * back to the compiler itself.  This ensures that MPI-wrapper compilers
     * (e.g. mpicc, mpicxx) are used when MPI is present, while regular
     * compilers are used otherwise.
     *
     * @param property          The property name to look up (e.g. "c", "cxx",
     *                          "fort").
     * @param dependencies      The resolved concrete dependency set; used to
     *                          check for MPI presence.
     * @param abstract_packages Mapping from abstract to concrete package names.
     * @param compiler          Raw compiler string forwarded to
     *                          has_template_property().
     * @return A template reference string "${mpi.property}" if MPI is active
     *         and exposes the property; "${compiler.property}" if the compiler
     *         exposes it; otherwise an empty string.
     *
     * @warning Terminates the program via has_template_property() /
     *          get_db_config() if the MPI or compiler recipe is missing.
     */
    std::string compiler_property(const std::string& property,
                                  const std::vector<std::string>& dependencies,
                                  const AbstractPackageSelections& abstract_packages,
                                  const std::string& compiler) {
        if (std::find(dependencies.begin(), dependencies.end(), "mpi") != dependencies.end() &&
            has_template_property("mpi", property, abstract_packages, compiler)) {
            return template_value("mpi", property);
        }
        if (has_template_property("compiler", property, abstract_packages, compiler)) {
            return template_value("compiler", property);
        }
        return {};
    }

    /**
     * @brief Collect the canonical keys of every option the user has explicitly
     *        configured in the build configuration.
     *
     * For each BuildOption in the configuration, the function derives the
     * toolchain-agnostic key via option_key() on:
     *   - the option's own `name` field,
     *   - the option's `enabled_format` (if set), and
     *   - the option's `disabled_format` (if set).
     *
     * The returned set is used by append_default() to avoid injecting a
     * default option whose key the user has already explicitly set.
     *
     * @param configuration The build configuration whose options are examined.
     * @param toolchain     The build toolchain, forwarded to option_key() for
     *                      correct prefix stripping.
     * @return An unordered_set of canonical option keys that already have an
     *         explicit configuration.
     */
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

    /**
     * @brief Append a default BuildOption to a configuration if the user has
     *        not already supplied an option with the same canonical key.
     *
     * If @p value is empty, the function returns immediately (no default worth
     * injecting).  Otherwise it computes the canonical key of the proposed
     * default via option_key() and checks whether that key already appears in
     * the @p explicit_options set.  If it does, the user's explicit choice is
     * preserved and the default is skipped.
     *
     * The appended option is marked as user-configurable, has no conditions,
     * and carries the given @p value as its default enabled value.  The name
     * is stored in the raw toolchain-specific form (e.g. "--prefix" or
     * "-DCMAKE_INSTALL_PREFIX") so that downstream serialisation emits the
     * correct flag syntax.
     *
     * @param configuration    The build configuration to mutate in-place.
     * @param explicit_options Set of canonical keys that the user has already
     *                         explicitly configured.
     * @param toolchain        The build toolchain, forwarded to option_key()
     *                         for canonical-key computation.
     * @param name             The raw option name to emit (e.g. "--prefix" for
     *                         Autotools, "-DCMAKE_BUILD_TYPE:STRING=Release" for
     *                         CMake).
     * @param value            The default value to assign.  If empty or if an
     *                         equivalent key is already explicit, nothing is
     *                         appended.
     */
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

    /**
     * @brief Transform a raw build configuration by injecting toolchain-
     *        specific default options (compiler flags, install prefix, rpath,
     *        etc.) derived from the resolved dependency set.
     *
     * This is the core transformation applied to a package's BuildConfiguration
     * before it is emitted into the user-editable YAML config.  The function
     * operates only on Autotools and CMake toolchains; for other toolchains
     * (None, Make) the configuration is returned unchanged.
     *
     * The transformation proceeds in four steps:
     *   1. Compute the set of option keys the user has already explicitly
     *      configured (via explicit_option_keys()) so that manually-provided
     *      values are never overwritten.
     *   2. Compute the aggregated DependencyDefaults (include flags, linker
     *      flags, library names, library paths) from the compiler and all
     *      active dependencies.
    *   3. Compute the list of active dependency names.
    *   4. Inject a standard set of default options depending on the toolchain:
     *
     *      **Autotools defaults injected:**
     *        prefix, CC, CXX, FC, CPPFLAGS, CFLAGS, CXXFLAGS, FCFLAGS, LDFLAGS
     *
     *      **CMake defaults injected:**
     *        CMAKE_INSTALL_PREFIX, CMAKE_BUILD_TYPE (Release),
     *        CMAKE_C_COMPILER, CMAKE_CXX_COMPILER, CMAKE_Fortran_COMPILER,
     *        CMAKE_C_FLAGS, CMAKE_CXX_FLAGS, CMAKE_Fortran_FLAGS,
     *        CMAKE_CUDA_FLAGS, CMAKE_EXE_LINKER_FLAGS,
     *        CMAKE_SHARED_LINKER_FLAGS, CMAKE_MODULE_LINKER_FLAGS,
     *        CMAKE_BUILD_RPATH, CMAKE_INSTALL_RPATH
     *
     * Compiler references are resolved via compiler_property(), which prefers
     * MPI wrappers when MPI is an active dependency.
     *
     * @param configuration     The raw BuildConfiguration from the package
     *                          recipe to transform.
     * @param package           The PackageConfig of the package being built.
     *                          Its name and properties are used for generating
     *                          default prefix and library-path values.
     * @param toolchain         The build toolchain (Autotools or CMake); for
     *                          other values the configuration is returned
     *                          unchanged.
     * @param dependencies      The resolved concrete dependency set.
     * @param abstract_packages Mapping from abstract to concrete package names.
     * @param compiler          The raw compiler string (e.g. "gcc@12") used to
     *                          look up compiler properties via
     *                          compiler_property().
     * @return A copy of @p configuration with toolchain-specific default
     *         options appended (unless the user already explicitly provided an
     *         equivalent option).
     *
     * @warning Terminates the program via has_template_property() /
     *          get_db_config() if any package recipe needed for property lookup
     *          is missing from the database.
     *
     * @see append_default()       Injects a single default option, respecting
     *                             the user's explicit choices.
     * @see dependency_defaults()  Aggregates compiler and dependency flags.
     * @see compiler_property()    Resolves compiler/MPI binary properties.
     */
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
        }

        return result;
    }

    /**
     * @brief Transform a package's Build specification by resolving the
     *        configuration sub-block through transformed_configuration().
     *
     * If the package has no build specification (build is std::nullopt), the
     * function returns std::nullopt immediately.  Otherwise it copies the
     * Build and, if a configurations sub-block is present, applies
     * transformed_configuration() to it using the package's own toolchain,
     * dependency set, abstract-package selections, and compiler setting.
     *
     * The build's preprocessing, postprocessing, and stages are passed through
     * unchanged -- only the top-level configurations block is transformed.
     *
     * @param package              The PackageConfig whose Build object is to be
     *                             transformed.
     * @param dependencies         The resolved concrete dependency set.
     * @param abstract_packages    Mapping from abstract package names to
     *                             concrete implementations.
     * @param compiler             The raw compiler string forwarded to
     *                             transformed_configuration().
     * @return A fully resolved Build object with its configuration transformed,
     *         or std::nullopt if the package defines no build specification.
     *
     * @warning Terminates the program via transformed_configuration() if any
     *          needed package recipe is missing from the database.
     */
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
