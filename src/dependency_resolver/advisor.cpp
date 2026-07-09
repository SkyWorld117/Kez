#include <yaml-cpp/yaml.h>

#include <cstdlib>
#include <dependency_resolver/advisor.hpp>
#include <filesystem>
#include <utils/bash_utils.hpp>
#include <utils/colored_io.hpp>
#include <utils/yaml_utils.hpp>

/**
 * @brief Consult the heuristics advice table to resolve an abstract package name
 *        to an architecture-appropriate concrete package name.
 *
 * Loads the advice YAML file from `<KEZ_HOME>/heuristics/advice.yaml` via the
 * process-wide cached YAML loader, then looks up the given abstract package name
 * under the top-level `advice` map.  Within that entry, the architecture
 * identifier (read from the `KEZ_ARCH` environment variable) selects the final
 * concrete package name to return.
 *
 * ### Lookup logic
 *
 * The YAML document is expected to have the following structure:
 *
 * ```yaml
 * advice:
 *   <abstract_package>:          # e.g. "blas", "lapack", "mpi", "fftw3-api"
 *     <architecture>: <concrete> # e.g. "x86_64" -> "intel-oneapi-mkl"
 * ```
 *
 * 1. If the top-level `advice` map contains a key matching `abstract_package`,
 *    descend into that entry.
 * 2. If that entry contains a key matching the current `KEZ_ARCH` value, return
 *    the corresponding scalar value as the concrete package name.
 * 3. Otherwise the program terminates with a fatal error.
 *
 * @param abstract_package  The abstract/generic package name to resolve
 *                          (e.g. "blas", "lapack", "fftw3-api", "mpi").
 *                          Must be a top-level key under the `advice` map in
 *                          `<KEZ_HOME>/heuristics/advice.yaml`.
 *
 * @return The architecture-specific concrete package name string (e.g.
 *         "intel-oneapi-mkl", "nvpl", "openmpi").  The caller owns the returned
 *         string and should use it to replace the abstract dependency in the
 *         resolved plan.
 *
 * @warning Terminates the process with exit(EXIT_FAILURE) via the ERROR macro if:
 *          - `KEZ_HOME` or `KEZ_ARCH` environment variables are unset (cascaded
 *            from the underlying utility functions);
 *          - The advice YAML file cannot be found, read, or parsed (cascaded
 *            from cached_yaml_load());
 *          - No entry exists in the advice table for the given
 *            (`abstract_package`, `architecture`) pair (line 24).
 *
 * @note The advice YAML file is loaded with cache-aware semantics via
 *       cached_yaml_load(), so repeated calls within the same process incur
 *       disk I/O only on the first invocation.
 *
 * @see get_env_var         Reads `KEZ_HOME` and `KEZ_ARCH` from the process
 *                          environment.
 * @see cached_yaml_load    Underlying YAML loader (may return a stale cache
 *                          entry; see its contract for cache-invalidation
 *                          guarantees).
 * @see yaml_has            Checks key existence in a YAML node.
 * @see yaml_scalar         Extracts a scalar value, erroring on missing data.
 * @see heuristics/advice.yaml  The data file consulted by this function.
 */
std::string advise(const std::string& abstract_package) {
    const std::string kez_home     = get_env_var("KEZ_HOME");
    const std::string architecture = get_env_var("KEZ_ARCH");

    const std::filesystem::path advice_path =
        std::filesystem::path(kez_home) / "heuristics" / "advice.yaml";

    const YAML::Node advice = cached_yaml_load(advice_path)["advice"];
    if (yaml_has(advice, abstract_package) && yaml_has(advice[abstract_package], architecture)) {
        return yaml_scalar(advice[abstract_package][architecture],
                           "dependency advice for " + abstract_package);
    }

    ERROR("No dependency advice for package '" + abstract_package + "' on architecture '" +
          architecture + "'");
    exit(EXIT_FAILURE);
}
