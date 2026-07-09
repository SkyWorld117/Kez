#include <database/config.hpp>

namespace {
    /**
     * @brief Build a make command string with optional parallel-jobs and
     *        target arguments.
     *
     * Constructs a command of the form ``make [-j<N>] [<target>]``.  The
     * parallel flag is inserted only when the stage permits multithreaded
     * execution (@ref BuildStage::multithreaded) and the caller supplies a
     * non-zero job count.
     *
     * @param stage         The build stage descriptor from which the target
     *                      name and multithreading flag are read.
     * @param parallel_jobs Number of parallel ``make`` jobs to request.
     *                      A value of zero suppresses the ``-j`` flag
     *                      regardless of the stage's multithreaded setting.
     * @return A fully formed make command string (e.g. ``"make -j4 all"``).
     */
    std::string make_stage_command(const BuildStage& stage, unsigned int parallel_jobs) {
        std::string command = "make";
        if (stage.multithreaded && parallel_jobs != 0) {
            command += " -j" + std::to_string(parallel_jobs);
        }
        if (stage.target.has_value() && !stage.target->empty()) {
            command += " " + *stage.target;
        }
        return command;
    }
}  // namespace

/**
 * @brief Base-class default: no toolchain-specific configure command.
 *
 * Subclasses (e.g. @ref AutotoolsPackageConfig) override this to return a
 * sensible default such as ``"./configure"`` or ``"cmake -B build"``.
 *
 * @return Always @c std::nullopt, indicating that no default configure
 *         command exists for the unspecialized @ref PackageConfig.
 */
std::optional<std::string> PackageConfig::default_configuration_command() const {
    return std::nullopt;
}

/**
 * @brief Base-class default: no toolchain-specific build-stage command.
 *
 * Subclasses (e.g. @ref AutotoolsPackageConfig, @ref CMakePackageConfig)
 * override this to produce commands like ``"make -j4"`` or
 * ``"cmake --build build --parallel 4"`` based on the stage and job count.
 *
 * @param stage         The build stage for which a command is needed
 *                      (ignored by the base implementation).
 * @param parallel_jobs Number of parallel jobs to use
 *                      (ignored by the base implementation).
 * @return Always @c std::nullopt, indicating that no default stage command
 *         exists for the unspecialized @ref PackageConfig.
 */
std::optional<std::string> PackageConfig::default_stage_command(const BuildStage&,
                                                                unsigned int) const {
    return std::nullopt;
}

/**
 * @brief Identify the build-system toolchain as absent / unspecified.
 *
 * @ref GenericPackageConfig is used when no toolchain is declared in the
 * recipe YAML.  It provides no default configure or build commands; all
 * build steps must be supplied explicitly through the @ref Build structure.
 *
 * @return @ref Toolchain::None.
 */
Toolchain GenericPackageConfig::toolchain() const noexcept { return Toolchain::None; }

/**
 * @brief Identify the build-system toolchain as GNU Autotools.
 *
 * @return @ref Toolchain::Autotools.
 */
Toolchain AutotoolsPackageConfig::toolchain() const noexcept { return Toolchain::Autotools; }

/**
 * @brief Return the default configure command for an Autotools package.
 *
 * @return The string ``"./configure"``.
 */
std::optional<std::string> AutotoolsPackageConfig::default_configuration_command() const {
    return "./configure";
}

/**
 * @brief Return a default build-stage command for an Autotools package.
 *
 * Delegates to the internal @ref make_stage_command helper to produce a
 * ``make`` invocation with optional parallel-jobs and target arguments.
 *
 * @param stage         The build stage descriptor (target and
 *                      multithreading flag are read from this).
 * @param parallel_jobs Number of parallel ``make`` jobs; zero suppresses
 *                      the ``-j`` flag.
 * @return A command string such as ``"make -j4"`` or ``"make install"``.
 */
std::optional<std::string> AutotoolsPackageConfig::default_stage_command(
    const BuildStage& stage, unsigned int parallel_jobs) const {
    return make_stage_command(stage, parallel_jobs);
}

/**
 * @brief Identify the build-system toolchain as CMake.
 *
 * @return @ref Toolchain::CMake.
 */
Toolchain CMakePackageConfig::toolchain() const noexcept { return Toolchain::CMake; }

/**
 * @brief Return the default configure command for a CMake package.
 *
 * @return The string ``"cmake -B build"`` (generates the build directory
 *         and configures the project in one step).
 */
std::optional<std::string> CMakePackageConfig::default_configuration_command() const {
    return "cmake -B build";
}

/**
 * @brief Return a default build-stage command for a CMake package.
 *
 * Produces platform-appropriate CMake invocations:
 *   - For an @c install target: ``"cmake --install build"``.
 *   - For all other stages: ``"cmake --build build"`` optionally augmented
 *     with ``--parallel <N>`` (when the stage permits multithreading and
 *     @p parallel_jobs is non-zero) and ``--target <target>`` (when a
 *     non-empty target is set on the stage).
 *
 * @param stage         The build stage descriptor whose target and
 *                      multithreading flag guide the command construction.
 * @param parallel_jobs Number of parallel build jobs; zero suppresses the
 *                      ``--parallel`` flag.
 * @return A command string such as ``"cmake --build build --parallel 4"``,
 *         ``"cmake --build build --target mylib"``, or
 *         ``"cmake --install build"``.
 */
std::optional<std::string> CMakePackageConfig::default_stage_command(
    const BuildStage& stage, unsigned int parallel_jobs) const {
    if (stage.target == "install") {
        return "cmake --install build";
    }

    std::string command = "cmake --build build";
    if (stage.multithreaded && parallel_jobs != 0) {
        command += " --parallel " + std::to_string(parallel_jobs);
    }
    if (stage.target.has_value() && !stage.target->empty()) {
        command += " --target " + *stage.target;
    }
    return command;
}

/**
 * @brief Identify the build-system toolchain as plain Make.
 *
 * @return @ref Toolchain::Make.
 */
Toolchain MakePackageConfig::toolchain() const noexcept { return Toolchain::Make; }

/**
 * @brief Return a default build-stage command for a plain Makefile package.
 *
 * Delegates to the internal @ref make_stage_command helper to produce a
 * ``make`` invocation with optional parallel-jobs and target arguments.
 * No default configure command is provided because plain Makefile projects
 * typically do not have a separate configure step.
 *
 * @param stage         The build stage descriptor (target and
 *                      multithreading flag are read from this).
 * @param parallel_jobs Number of parallel ``make`` jobs; zero suppresses
 *                      the ``-j`` flag.
 * @return A command string such as ``"make -j4"`` or ``"make install"``.
 */
std::optional<std::string> MakePackageConfig::default_stage_command(
    const BuildStage& stage, unsigned int parallel_jobs) const {
    return make_stage_command(stage, parallel_jobs);
}
