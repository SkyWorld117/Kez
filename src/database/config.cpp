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

std::optional<std::string> PackageConfig::default_configuration_command() const {
    return std::nullopt;
}

std::optional<std::string> PackageConfig::default_stage_command(const BuildStage&,
                                                                unsigned int) const {
    return std::nullopt;
}

Toolchain GenericPackageConfig::toolchain() const noexcept { return Toolchain::None; }

Toolchain AutotoolsPackageConfig::toolchain() const noexcept { return Toolchain::Autotools; }

std::optional<std::string> AutotoolsPackageConfig::default_configuration_command() const {
    return "./configure";
}

std::optional<std::string> AutotoolsPackageConfig::default_stage_command(
    const BuildStage& stage, unsigned int parallel_jobs) const {
    return make_stage_command(stage, parallel_jobs);
}

Toolchain CMakePackageConfig::toolchain() const noexcept { return Toolchain::CMake; }

std::optional<std::string> CMakePackageConfig::default_configuration_command() const {
    return "cmake -B build";
}

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

Toolchain MakePackageConfig::toolchain() const noexcept { return Toolchain::Make; }

std::optional<std::string> MakePackageConfig::default_stage_command(
    const BuildStage& stage, unsigned int parallel_jobs) const {
    return make_stage_command(stage, parallel_jobs);
}
