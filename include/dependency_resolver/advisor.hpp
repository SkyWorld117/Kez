#pragma once

#include <string>

/**
 * @brief Consult the heuristics advice table to resolve an abstract package name
 *        to an architecture-appropriate concrete package name.
 *
 * Reads the `KEZ_HOME` and `KEZ_ARCH` environment variables at runtime, then
 * loads the YAML file at `<KEZ_HOME>/heuristics/advice.yaml`.  The file contains
 * a top-level `advice` map whose keys are abstract package names (e.g. "blas",
 * "lapack", "mpi") and whose values are architecture-keyed maps mapping an
 * architecture identifier (e.g. "x86_64", "arm64") to the name of a concrete
 * package (e.g. "intel-oneapi-mkl", "openmpi").
 *
 * If a matching entry is found, the corresponding concrete package name is
 * returned as a string.  If either the abstract package key is absent from the
 * advice map, or the architecture sub-key is absent for that package, the
 * program prints a fatal error and terminates with exit(EXIT_FAILURE).
 *
 * @param abstract_package  The abstract/generic package name to look up
 *                          (e.g. "blas", "lapack", "fftw3-api", "mpi").
 *                          Must correspond to a top-level key under the
 *                          `advice` map in advice.yaml.
 *
 * @return The architecture-specific concrete package name (e.g.
 *         "intel-oneapi-mkl", "nvpl", "openmpi").
 *
 * @warning Terminates the process with exit(EXIT_FAILURE) via the ERROR macro
 *          if the `KEZ_HOME` or `KEZ_ARCH` environment variables are unset,
 *          if the advice.yaml file cannot be read or parsed, or if no advice
 *          entry exists for the given (abstract_package, architecture) pair.
 *
 * @note The advice YAML file is loaded once per process lifetime via
 *       cached_yaml_load(); subsequent calls for different packages incur no
 *       additional I/O.
 *
 * @see cached_yaml_load    Underlying YAML loader with caching.
 * @see get_env_var         Reads the required environment variables.
 * @see heuristics/advice.yaml  The data file consulted by this function.
 * @see dependency_resolver  The component that uses this function to
 *                           disambiguate abstract dependencies.
 */
std::string advise(const std::string& abstract_package);
