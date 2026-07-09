#pragma once

#include <dependency_resolver/resolve_dependencies.hpp>
#include <string>
#include <unordered_set>
#include <vector>

/**
 * @brief Check whether all named requirements are satisfied by a set of dependencies.
 *
 * A requirement is satisfied if either:
 *   - it appears literally in @p dependencies, or
 *   - it is an abstract package name (e.g. "BLAS", "LAPACK") that maps through
 *     @p abstract_packages to a concrete package (e.g. "openblas") and that
 *     concrete name appears in @p dependencies.
 *
 * This overload accepts @p dependencies as a `std::vector<std::string>` and
 * performs a linear search.  Prefer the `unordered_set` overload when the
 * dependency list is large or this function is called repeatedly in a tight
 * loop.
 *
 * @param requirements       The list of requirement expressions to check.  Each
 *                           element is either a concrete package name or an
 *                           abstract package key (consulted via
 *                           @p abstract_packages).
 * @param dependencies       The concrete package names known to be present.
 *                           Scanned linearly via `std::find`.
 * @param abstract_packages  Mapping from abstract package names (e.g. "BLAS")
 *                           to concrete package names (e.g. "openblas").
 * @return true if every element of @p requirements is satisfied; false otherwise.
 *
 * @see requirements_satisfied(const std::vector<std::string>&,
 *                             const std::unordered_set<std::string>&,
 *                             const AbstractPackageSelections&)
 *     Overload accepting an unordered_set for O(1) average membership tests.
 *
 * @see AbstractPackageSelections  Type alias for the abstract-to-concrete
 *                                 package mapping (defined in
 *                                 resolve_dependencies.hpp).
 */
bool requirements_satisfied(const std::vector<std::string>& requirements,
                            const std::vector<std::string>& dependencies,
                            const AbstractPackageSelections& abstract_packages);

/**
 * @brief Check whether all named requirements are satisfied by a set of dependencies.
 *
 * Behaves identically to the vector overload, but accepts @p dependencies as
 * a `std::unordered_set<std::string>` for O(1) average membership lookups.
 * Use this version when the same dependency set is queried multiple times or
 * when the set is large enough that linear scans would be costly.
 *
 * @param requirements       The list of requirement expressions to check.  Each
 *                           element is either a concrete package name or an
 *                           abstract package key (consulted via
 *                           @p abstract_packages).
 * @param dependencies       The concrete package names known to be present,
 *                           stored as an unordered_set for fast lookup.
 * @param abstract_packages  Mapping from abstract package names (e.g. "BLAS")
 *                           to concrete package names (e.g. "openblas").
 * @return true if every element of @p requirements is satisfied; false otherwise.
 *
 * @see requirements_satisfied(const std::vector<std::string>&,
 *                             const std::vector<std::string>&,
 *                             const AbstractPackageSelections&)
 *     Overload accepting a vector (linear search).
 */
bool requirements_satisfied(const std::vector<std::string>& requirements,
                            const std::unordered_set<std::string>& dependencies,
                            const AbstractPackageSelections& abstract_packages);
