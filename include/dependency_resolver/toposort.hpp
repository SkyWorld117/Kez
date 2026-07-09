#pragma once

#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief Adjacency-list representation of a package dependency graph.
 *
 * Each key is a package name and its associated value is the list of packages
 * that the key depends on **directly**.  The graph must be acyclic (a DAG);
 * the topological_sort function will terminate the program if a cycle is
 * detected.
 *
 * @note  The direction is "package -> its dependencies".  This means
 *        dependencies appear **before** their dependents in a topological
 *        ordering derived from this graph.
 */
using DependencyGraph = std::unordered_map<std::string, std::vector<std::string>>;

/**
 * @brief Produce a topological ordering of the packages in the dependency
 *        graph.
 *
 * The returned vector is sorted so that every package appears after all of its
 * direct and transitive dependencies.  In other words, if package A depends on
 * package B, then B is placed before A in the result.
 *
 * @param adjacency_list  The dependency graph as a `DependencyGraph`
 *                        (adjacency list).  Every package that appears as a
 *                        dependency in any value list must also appear as a key
 *                        (possibly with an empty vector) so that the traversal
 *                        visits every node.
 *
 * @returns A vector of package names in topological order (dependencies first,
 *          dependents last).
 *
 * @warning  If the graph contains a cycle the function prints an error message
 *           via `ERROR()` and terminates the program with `EXIT_FAILURE`.
 *           No partial result is returned in this case.
 *
 * @see  resolve_dependencies  for building the graph from recipe metadata.
 * @see  DependencyGraph       for the required input format.
 */
std::vector<std::string> topological_sort(const DependencyGraph& adjacency_list);
