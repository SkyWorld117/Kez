#pragma once

#include <database/config.hpp>
#include <dependency_resolver/resolve_dependencies.hpp>
#include <optional>
#include <string>
#include <unordered_set>

namespace uconf_generator {

    /**
     * @brief Transforms a package's build configuration by applying overrides,
     *        resolving abstract package selections, and incorporating
     *        compiler- and toolchain-specific settings.
     *
     * This function takes the raw BuildConfiguration declared in a package recipe
     * and produces the final, concrete configuration that will be emitted into
     * the user-editable YAML config. The transformation includes:
     *   - Filtering and rewriting build options based on selected abstract
     *     packages (e.g. promoting a BLAS option to MKL or NVPL).
     *   - Applying any Override entries from the PackageConfig that target
     *     options or environment variables.
     *   - Injecting compiler-specific environment variables or flags.
     *   - Resolving conditional values (ConfigurableValue conditions) against
     *     the current dependency set and compiler.
     *
     * @param configuration   The raw BuildConfiguration from the package recipe.
     *                        May hold options, environment variables, and an
     *                        optional configuration command.
     * @param package         The PackageConfig whose overrides and properties
     *                        are consulted during the transformation. Must
     *                        outlive the call.
     * @param toolchain       The build toolchain in use (e.g. CMake, Autotools,
     *                        Make, or None). May alter how options are formatted
     *                        or which environment variables are injected.
     * @param dependencies    The set of resolved dependency package names that
     *                        the target package will be built against. Used to
     *                        evaluate conditional values and inject dependency-
     *                        derived flags.
     * @param abstract_packages  Mapping from abstract package names (e.g.
     *                           "BLAS", "LAPACK") to concrete package names
     *                           (e.g. "MKL", "NVPL") as resolved by the
     *                           dependency advisor. Options and environment
     *                           variables targeting abstract packages are
     *                           rewritten to refer to the concrete selection.
     * @param compiler        The name of the compiler that will be used to
     *                        build the package (e.g. "gcc", "llvm"). May
     *                        influence which options or environment variables
     *                        are included.
     *
     * @return A fully resolved BuildConfiguration ready to be serialised into
     *         the user-facing YAML config. The returned object embeds the
     *         concrete option names and values, resolved conditionals, and
     *         any injected toolchain/compiler settings.
     */
    BuildConfiguration transformed_configuration(
        const BuildConfiguration& configuration, const PackageConfig& package, Toolchain toolchain,
        const std::unordered_set<std::string>& dependencies,
        const AbstractPackageSelections& abstract_packages, const std::string& compiler);

    /**
     * @brief Transforms a package's Build specification into its final form
     *        by resolving abstract package selections, applying overrides,
     *        and incorporating compiler-specific adjustments.
     *
     * This function processes the top-level Build object of a package recipe
     * (which may contain preprocessing/postprocessing commands, a master
     * configuration block, and a list of build stages) and returns an
     * equivalent Build whose options, environment variables, and conditional
     * values have been fully resolved. The same transformations described in
     * transformed_configuration() are applied to each sub-configuration within
     * the Build.
     *
     * If the package has no build specification (i.e. `build` is std::nullopt),
     * the function returns std::nullopt. Similarly, if after transformation the
     * build becomes empty or inapplicable for the given dependency/compiler
     * context, the function may return std::nullopt.
     *
     * @param package              The PackageConfig whose build section is to
     *                             be transformed. Must outlive the call.
     * @param dependencies         The set of resolved dependency package names
     *                             the target will be built against.
     * @param abstract_packages    Mapping from abstract package names to
     *                             concrete selections.
     * @param compiler             The compiler name used to build the package.
     *
     * @return A fully resolved Build object, or std::nullopt if the package
     *         defines no build specification or if the build is not applicable
     *         in the current context.
     *
     * @see transformed_configuration()  Applied to each BuildConfiguration
     *                                   within the returned Build.
     */
    std::optional<Build> transformed_build(const PackageConfig& package,
                                           const std::unordered_set<std::string>& dependencies,
                                           const AbstractPackageSelections& abstract_packages,
                                           const std::string& compiler);

}  // namespace uconf_generator
