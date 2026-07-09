#include <algorithm>
#include <dependency_resolver/requirements.hpp>

namespace {
    /**
     * @brief Checks whether a list of abstract package requirements is satisfied
     *        by a concrete set of resolved dependencies.
     *
     * For each requirement in the list:
     *   1. If the requirement name itself appears in the concrete dependency set
     *      (e.g. the user directly selected that package), it is satisfied.
     *   2. Otherwise, look up the requirement in @p abstract_packages. If a
     *      concrete package name has been mapped to that abstract key, and that
     *      concrete name is present in the dependency set, the requirement is
     *      satisfied.
     *   3. If neither check passes, the requirement is **not** satisfied and the
     *      function returns false immediately.
     *
     * The notion of "contains" is parameterised via @p contains so that the same
     * logic works for both @c std::vector (linear scan) and @c std::unordered_set
     * (hash lookup) dependency containers without duplication.
     *
     * @tparam Dependencies  Container type holding concrete dependency names
     *                       (e.g. @c std::vector@<std::string@> or
     *                       @c std::unordered_set@<std::string@>).
     * @tparam Contains      Callable type with signature
     *                       `bool(const Dependencies&, const std::string&)`.
     * @param requirements   The list of abstract package requirements that must
     *                       be satisfied.
     * @param dependencies   The resolved concrete dependencies to check against.
     * @param abstract_packages  Map from abstract package names (e.g. "blas")
     *                           to the concrete package chosen for this
     *                           environment (e.g. "mkl").
     * @param contains       Functor that returns true if @p value is present in
     *                       @p dependencies.
     * @return true if every requirement is satisfied; false otherwise.
     *
     * @note This function does not terminate the program; it is a pure predicate.
     *       It performs no I/O and has no side effects.
     */
    template <typename Dependencies, typename Contains>
    bool requirements_satisfied_impl(const std::vector<std::string>& requirements,
                                     const Dependencies& dependencies,
                                     const AbstractPackageSelections& abstract_packages,
                                     Contains contains) {
        for (const std::string& requirement : requirements) {
            if (contains(dependencies, requirement)) {
                continue;
            }
            const auto selected = abstract_packages.find(requirement);
            if (selected == abstract_packages.end() || !contains(dependencies, selected->second)) {
                return false;
            }
        }
        return true;
    }
}  // namespace

/**
 * @brief Checks whether all abstract requirements are met by a concrete
 *        dependency vector.
 *
 * Convenience overload that delegates to @c requirements_satisfied_impl with
 * a linear-search containment check (std::find).
 *
 * @param requirements          The abstract requirements that must be satisfied.
 * @param dependencies          Concrete package names (as a vector) that have
 *                              been resolved.
 * @param abstract_packages     Map from abstract names to the concrete
 *                              selections made for this environment.
 * @return true if every abstract requirement is present in @p dependencies,
 *         either by its own name or through an abstract-package alias; false
 *         otherwise.
 *
 * @note This is a pure predicate with no side effects and no error-signalling
 *       calls (ERROR, fail_config, user_config_error, exit).
 */
bool requirements_satisfied(const std::vector<std::string>& requirements,
                            const std::vector<std::string>& dependencies,
                            const AbstractPackageSelections& abstract_packages) {
    return requirements_satisfied_impl(
        requirements, dependencies, abstract_packages,
        [](const std::vector<std::string>& values, const std::string& value) {
            return std::find(values.begin(), values.end(), value) != values.end();
        });
}

/**
 * @brief Checks whether all abstract requirements are met by a concrete
 *        dependency set (unordered).
 *
 * Convenience overload that delegates to @c requirements_satisfied_impl with
 * a hash-set containment check (std::unordered_set::find).
 *
 * @param requirements          The abstract requirements that must be satisfied.
 * @param dependencies          Concrete package names (as an unordered set)
 *                              that have been resolved.
 * @param abstract_packages     Map from abstract names to the concrete
 *                              selections made for this environment.
 * @return true if every abstract requirement is present in @p dependencies,
 *         either by its own name or through an abstract-package alias; false
 *         otherwise.
 *
 * @note This is a pure predicate with no side effects and no error-signalling
 *       calls (ERROR, fail_config, user_config_error, exit).
 */
bool requirements_satisfied(const std::vector<std::string>& requirements,
                            const std::unordered_set<std::string>& dependencies,
                            const AbstractPackageSelections& abstract_packages) {
    return requirements_satisfied_impl(
        requirements, dependencies, abstract_packages,
        [](const std::unordered_set<std::string>& values, const std::string& value) {
            return values.find(value) != values.end();
        });
}
