#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <factory/factory.hpp>
#include <string>
#include <utils/colored_io.hpp>
#include <utils/yaml_utils.hpp>
#include <vector>

namespace {
    struct FactoryRun {
        std::vector<std::string> commands;
        std::string summary_regex;
    };

    /**
     * @brief Require that a YAML node contains a given key with a scalar value,
     *        and return that value as a string.
     *
     * Terminates the program if the key is missing or its value is not a scalar.
     *
     * @param node         The parent YAML map to inspect.
     * @param key          The key whose value is required.
     * @param description  Human-readable context string used in error messages
     *                     (e.g. "Profile configuration").
     *
     * @return The string value of `node[key]`.
     *
     * @warning Calls `ERROR()` and `exit(EXIT_FAILURE)` if `key` is absent from
     *          `node` or if `node[key]` is not a scalar.
     */
    std::string require_scalar(const YAML::Node& node, const std::string& key,
                               const std::string& description) {
        if (!yaml_has(node, key) || !node[key].IsScalar()) {
            ERROR(description + " missing '" + key + "' key or '" + key + "' is not a scalar");
            exit(EXIT_FAILURE);
        }
        return yaml_scalar(node[key], description + " " + key);
    }

    /**
     * @brief Replace all occurrences of a substring within a string, in place.
     *
     * This is a simple find-and-replace loop. It safely handles overlapping
     * patterns by advancing `position` past the replacement text after each
     * substitution.
     *
     * @param value        The string to modify in place.
     * @param pattern      The substring to search for.
     * @param replacement  The string to substitute for each match.
     */
    void replace_all(std::string& value, const std::string& pattern,
                     const std::string& replacement) {
        std::size_t position = 0;
        while ((position = value.find(pattern, position)) != std::string::npos) {
            value.replace(position, pattern.size(), replacement);
            position += replacement.size();
        }
    }

    /**
     * @brief Resolve a scalar value through the three-level inheritance chain:
     *        factory -> buildspace -> profile.
     *
     * At each level the `@p key` is looked up in the corresponding YAML map.
     * If the key exists and is not null, its value overrides the inherited
     * value from the parent level. The special placeholder `${<key>}` in the
     * child's value is replaced with the parent's value, enabling patterns
     * such as appending to a base path.
     *
     * The resolution order is:
     *   1. factory_config   (base value)
     *   2. buildspace_config (overrides factory, may reference ${key})
     *   3. profile_config    (overrides buildspace, may reference ${key})
     *
     * @param factory_config     The top-level factory YAML map.
     * @param buildspace_config  The current buildspace YAML map.
     * @param profile_config     The current profile YAML map.
     * @param key                The configuration key to resolve.
     *
     * @return The resolved string value. Returns an empty string if the key is
     *         absent at all three levels.
     *
     * @see inherited_resource  Wraps this function with a default fallback.
     */
    std::string inherited_scalar(const YAML::Node& factory_config,
                                 const YAML::Node& buildspace_config,
                                 const YAML::Node& profile_config, const std::string& key) {
        std::string value;
        if (yaml_has(factory_config, key) && !factory_config[key].IsNull()) {
            value = yaml_scalar(factory_config[key], "factory " + key);
        }
        if (yaml_has(buildspace_config, key) && !buildspace_config[key].IsNull()) {
            const std::string parent = value;
            value                    = yaml_scalar(buildspace_config[key], "buildspace " + key);
            replace_all(value, "${" + key + "}", parent);
        }
        if (yaml_has(profile_config, key) && !profile_config[key].IsNull()) {
            const std::string parent = value;
            value                    = yaml_scalar(profile_config[key], "profile " + key);
            replace_all(value, "${" + key + "}", parent);
        }
        return value;
    }

    /**
     * @brief Extract the factory configuration body from a YAML node.
     *
     * If the node contains a `factory` key, its value is treated as the
     * configuration root. Otherwise the node itself is used directly.
     * This allows the factory config to be specified either as:
     *
     *   factory:
     *     buildspace: [...]
     *
     * or equivalently (when the document is already the factory map):
     *
     *   buildspace: [...]
     *
     * @param config  The top-level YAML node from the configuration file.
     *
     * @return The YAML node representing the factory configuration body.
     */
    YAML::Node factory_body(const YAML::Node& config) {
        if (yaml_has(config, "factory")) {
            return config["factory"];
        }
        return config;
    }

    /**
     * @brief Find a named entry in a YAML sequence of maps.
     *
     * Performs a linear search through `sequence` for a map item whose
     * `name` key has the scalar value equal to `name`. Returns the first
     * match. This is used to locate sibling buildspace and profile entries.
     *
     * @param sequence     The YAML sequence to search.
     * @param name         The value of the `name` field to look for.
     * @param description  Human-readable context string for error messages.
     *
     * @return The matching YAML map node, or a null (undefined) node if no
     *         match is found.
     *
     * @warning Terminates the program if `sequence` is not a YAML sequence.
     */
    YAML::Node find_named_entry(const YAML::Node& sequence, const std::string& name,
                                const std::string& description) {
        if (!sequence.IsSequence()) {
            ERROR(description + " must be a sequence");
            exit(EXIT_FAILURE);
        }
        for (const YAML::Node& item : sequence) {
            if (!item.IsMap() || !yaml_has(item, "name") || !item["name"].IsScalar()) {
                continue;
            }
            if (yaml_scalar(item["name"], description + " name") == name) {
                return item;
            }
        }
        return YAML::Node();
    }

    /**
     * @brief Detect and reject a configuration cycle.
     *
     * Checks whether `name` already appears in `stack`. If it does, a cycle
     * exists in the sibling inheritance chain and the function terminates the
     * program with an error. This is used both for buildspace-level and
     * profile-level sibling traversal.
     *
     * @param stack        The current ancestry stack of names visited during
     *                     traversal (most recent last).
     * @param name         The name to check for duplication.
     * @param description  Human-readable label for error messages
     *                     ("Profile" or "Cellar").
     *
     * @warning Terminates the program if `name` is already present in `stack`,
     *          indicating a cyclic sibling reference.
     */
    void reject_cycle(const std::vector<std::string>& stack, const std::string& name,
                      const std::string& description) {
        if (std::find(stack.begin(), stack.end(), name) != stack.end()) {
            ERROR(description + " sibling cycle includes '" + name + "'");
            exit(EXIT_FAILURE);
        }
    }

    /**
     * @brief Parse a string as an unsigned long integer with validation.
     *
     * Converts the string via `std::strtoul` and validates that the entire
     * string was consumed, the conversion succeeded, and the result satisfies
     * the zero-allowed policy.
     *
     * @param value       The string to parse.
     * @param key         The configuration key name, used in error messages.
     * @param allow_zero  If true, zero is a valid value; otherwise only
     *                    positive integers are accepted.
     *
     * @return The parsed unsigned long integer.
     *
     * @warning Terminates the program if the string is not a valid non-negative
     *          (or positive) integer, or if `strtoul` fails.
     */
    unsigned long parse_unsigned_resource(const std::string& value, const std::string& key,
                                          bool allow_zero) {
        errno                      = 0;
        char* end                  = nullptr;
        const unsigned long parsed = std::strtoul(value.c_str(), &end, 10);
        if (errno != 0 || end == value.c_str() || *end != '\0' || (!allow_zero && parsed == 0)) {
            ERROR("Factory resource field '" + key + "' must be " +
                  std::string(allow_zero ? "a non-negative" : "a positive") + " integer");
            exit(EXIT_FAILURE);
        }
        return parsed;
    }

    /**
     * @brief Resolve a resource value through inheritance with a default fallback.
     *
     * Delegates to @ref inherited_scalar to resolve `key` across the three
     * configuration levels. If the resolved value is empty, `default_value`
     * is returned instead.
     *
     * @param factory_config     Top-level factory YAML map.
     * @param buildspace_config  Current buildspace YAML map.
     * @param profile_config     Current profile YAML map.
     * @param key                The configuration key to resolve.
     * @param default_value      Fallback value when inheritance yields an
     *                           empty string (e.g. "1" for num_nodes).
     *
     * @return The resolved string value, or `default_value` if empty.
     *
     * @see inherited_scalar
     */
    std::string inherited_resource(const YAML::Node& factory_config,
                                   const YAML::Node& buildspace_config,
                                   const YAML::Node& profile_config, const std::string& key,
                                   const std::string& default_value) {
        std::string value =
            inherited_scalar(factory_config, buildspace_config, profile_config, key);
        return value.empty() ? default_value : value;
    }

    /**
     * @brief Build the bash command to copy input files into the current
     *        working directory.
     *
     * Produces `cp -a <inputs>/. .` which recursively copies the contents of
     * the inputs directory (preserving permissions, symlinks, etc.) into the
     * current directory. The trailing `/.` ensures only the *contents* of the
     * inputs directory are copied, not the directory itself.
     *
     * @param inputs  Path to the input data directory.
     *
     * @return A bash command string for copying inputs.
     */
    std::string copy_inputs_command(const std::string& inputs) {
        return "cp -a " + inputs + "/. .";
    }

    /**
     * @brief Parse a single factory profile from YAML and produce its
     *        command sequence.
     *
     * A profile defines how to run a benchmarking or profiling workload:
     * what inputs to copy, what prerun/postrun scripts to execute, and —
     * critically — what target command to run, along with the launcher,
     * scheduler, and resource allocation parameters.
     *
     * Inheritance is resolved from factory and buildspace levels via
     * @ref inherited_scalar / @ref inherited_resource. If the profile has a
     * `sibling` key, processing is delegated to the named sibling profile
     * instead (used for sharing configuration between similar profiles).
     * Cycles in sibling references are detected and rejected.
     *
     * The resulting command sequence (in order) is:
     *   1. Input copy (`cp -a <inputs>/. .`)  -- if `inputs` is defined
     *   2. Prerun script                       -- if `prerun` is defined
     *   3. Launcher/scheduler-wrapped target   -- always present; built by
     *      @ref wrap_factory_target
     *   4. Postrun script                      -- if `postrun` is defined
     *
     * @param factory_config     Top-level factory YAML map.
     * @param buildspace_config  The buildspace YAML map containing this
     *                           profile.
     * @param profile_config     The profile's YAML map to parse.
     * @param stack              Current ancestry stack for cycle detection
     *                           (profile names visited so far).
     *
     * @return A @ref FactoryRun struct containing the ordered command list
     *         and the summary regex (or empty string if unspecified).
     *
     * @warning Terminates the program if:
     *          - profile_config is not a map
     *          - the `name` key is missing or not a scalar
     *          - a sibling reference forms a cycle
     *          - the sibling profile does not exist
     *          - the `target` key is undefined at all three inheritance levels
     */
    FactoryRun parse_profile_run(const YAML::Node& factory_config,
                                 const YAML::Node& buildspace_config,
                                 const YAML::Node& profile_config, std::vector<std::string> stack) {
        if (!profile_config.IsMap()) {
            ERROR("Profile configuration should be a map");
            exit(EXIT_FAILURE);
        }
        const std::string name = require_scalar(profile_config, "name", "Profile configuration");
        reject_cycle(stack, name, "Profile");
        stack.push_back(name);

        if (yaml_has(profile_config, "sibling")) {
            const std::string sibling_name =
                yaml_scalar(profile_config["sibling"], "profile sibling");
            const YAML::Node sibling =
                find_named_entry(buildspace_config["profiles"], sibling_name, "profiles");
            if (!sibling.IsDefined()) {
                ERROR("Profile sibling does not exist: " + sibling_name);
                exit(EXIT_FAILURE);
            }
            return parse_profile_run(factory_config, buildspace_config, sibling, stack);
        }

        FactoryRun run;
        const std::string inputs =
            inherited_scalar(factory_config, buildspace_config, profile_config, "inputs");
        if (inputs.empty()) {
            WARNING("'inputs' key is not defined.");
        } else {
            run.commands.push_back(copy_inputs_command(inputs));
        }

        const std::string prerun =
            inherited_scalar(factory_config, buildspace_config, profile_config, "prerun");
        if (!prerun.empty()) {
            run.commands.push_back(prerun);
        }

        const std::string target =
            inherited_scalar(factory_config, buildspace_config, profile_config, "target");
        if (target.empty()) {
            ERROR("'target' key is not defined.");
            exit(EXIT_FAILURE);
        }

        std::string launcher =
            inherited_scalar(factory_config, buildspace_config, profile_config, "launcher");
        if (launcher.empty()) {
            launcher = "none";
        }
        const std::string launcher_opts =
            inherited_scalar(factory_config, buildspace_config, profile_config, "launcher_opts");

        std::string scheduler =
            inherited_scalar(factory_config, buildspace_config, profile_config, "scheduler");
        if (scheduler.empty()) {
            scheduler = "none";
        }
        const std::string scheduler_opts =
            inherited_scalar(factory_config, buildspace_config, profile_config, "scheduler_opts");

        const std::string num_nodes =
            inherited_resource(factory_config, buildspace_config, profile_config, "num_nodes", "1");
        const std::string num_procs_per_node = inherited_resource(
            factory_config, buildspace_config, profile_config, "num_procs_per_node", "1");
        const std::string cores_per_proc = inherited_resource(
            factory_config, buildspace_config, profile_config, "cores_per_proc", "1");
        const std::string omp_num_threads = inherited_resource(
            factory_config, buildspace_config, profile_config, "omp_num_threads", cores_per_proc);
        const std::string gpus_per_proc = inherited_resource(factory_config, buildspace_config,
                                                             profile_config, "gpus_per_proc", "0");

        run.commands.push_back(wrap_factory_target(target, launcher, launcher_opts, scheduler,
                                                   scheduler_opts, num_nodes, num_procs_per_node,
                                                   cores_per_proc, omp_num_threads, gpus_per_proc));

        const std::string postrun =
            inherited_scalar(factory_config, buildspace_config, profile_config, "postrun");
        if (!postrun.empty()) {
            run.commands.push_back(postrun);
        }

        run.summary_regex =
            inherited_scalar(factory_config, buildspace_config, profile_config, "summary_regex");
        return run;
    }

    /**
     * @brief Parse all profiles within a buildspace from YAML.
     *
     * Validates the buildspace configuration (must be a map with a `name`
     * scalar), detects and rejects sibling cycles, and handles sibling
     * delegation (if the buildspace's `sibling` key is set, its profiles are
     * parsed from the named sibling instead). Each profile in the
     * `profiles` sequence is parsed via @ref parse_profile_run.
     *
     * @param factory_config     Top-level factory YAML map.
     * @param buildspace_config  The buildspace YAML map whose `profiles`
     *                           sequence (or sibling's) will be parsed.
     * @param stack              Current ancestry stack for sibling cycle
     *                           detection (buildspace names visited so far).
     *
     * @return A vector of @ref FactoryProfile structs with all inheritance
     *         resolved and commands assembled.
     *
     * @warning Terminates the program if:
     *          - buildspace_config is not a map
     *          - the `name` key is missing or not a scalar
     *          - a sibling reference forms a cycle
     *          - the sibling buildspace does not exist
     *          - the `profiles` key is missing or is not a sequence
     *          - any individual profile fails parsing (delegated to
     *            @ref parse_profile_run).
     */
    std::vector<FactoryProfile> parse_buildspace_profiles(const YAML::Node& factory_config,
                                                          const YAML::Node& buildspace_config,
                                                          std::vector<std::string> stack) {
        if (!buildspace_config.IsMap()) {
            ERROR("Cellar configuration should be a map");
            exit(EXIT_FAILURE);
        }
        const std::string name = require_scalar(buildspace_config, "name", "Cellar configuration");
        reject_cycle(stack, name, "Cellar");
        stack.push_back(name);

        if (yaml_has(buildspace_config, "sibling")) {
            const std::string sibling_name =
                yaml_scalar(buildspace_config["sibling"], "buildspace sibling");
            const YAML::Node sibling =
                find_named_entry(factory_config["buildspace"], sibling_name, "buildspace");
            if (!sibling.IsDefined()) {
                ERROR("Cellar sibling does not exist: " + sibling_name);
                exit(EXIT_FAILURE);
            }
            return parse_buildspace_profiles(factory_config, sibling, stack);
        }

        if (!yaml_has(buildspace_config, "profiles")) {
            ERROR("Cellar configuration missing 'profiles' key");
            exit(EXIT_FAILURE);
        }
        if (!buildspace_config["profiles"].IsSequence()) {
            ERROR("'profiles' key should be a sequence");
            exit(EXIT_FAILURE);
        }

        std::vector<FactoryProfile> profiles;
        for (const YAML::Node& profile_config : buildspace_config["profiles"]) {
            const std::string profile_name =
                require_scalar(profile_config, "name", "Profile configuration");
            INFO("  Parsing profile: " + profile_name);
            const FactoryRun run = parse_profile_run(factory_config, buildspace_config,
                                                     profile_config, std::vector<std::string>());
            profiles.push_back({profile_name, run.commands, run.summary_regex});
        }
        return profiles;
    }

    /**
     * @brief Append an option string to a command, followed by a space, if
     *        the option is non-empty.
     *
     * This is a small helper used when assembling launcher and scheduler
     * command lines. It avoids trailing-spacing or extra-space issues by
     * only adding content when the option is present.
     *
     * @param command  The command string being built (modified in place).
     * @param option   The option string to append. If empty, nothing is
     *                 appended.
     */
    void append_if_present(std::string& command, const std::string& option) {
        if (!option.empty()) {
            command += option + " ";
        }
    }
}  // namespace

FactoryPlan parse_factory_config(const YAML::Node& config) {
    const YAML::Node factory_config = factory_body(config);
    if (!factory_config.IsMap()) {
        ERROR("Factory configuration should be a map");
        exit(EXIT_FAILURE);
    }
    if (!yaml_has(factory_config, "buildspace") || !factory_config["buildspace"].IsSequence()) {
        ERROR("'buildspace' key should be a sequence");
        exit(EXIT_FAILURE);
    }

    FactoryPlan plan;
    for (const YAML::Node& buildspace_config : factory_config["buildspace"]) {
        const std::string buildspace_name =
            require_scalar(buildspace_config, "name", "Cellar configuration");
        INFO("Parsing buildspace: " + buildspace_name);
        plan.push_back(
            {buildspace_name, parse_buildspace_profiles(factory_config, buildspace_config,
                                                        std::vector<std::string>())});
    }
    return plan;
}

std::string wrap_factory_target(const std::string& target, const std::string& launcher,
                                const std::string& launcher_opts, const std::string& scheduler,
                                const std::string& scheduler_opts, const std::string& num_nodes,
                                const std::string& num_procs_per_node,
                                const std::string& cores_per_proc,
                                const std::string& omp_num_threads,
                                const std::string& gpus_per_proc) {
    const unsigned long nodes = parse_unsigned_resource(num_nodes, "num_nodes", false);
    const unsigned long procs_per_node =
        parse_unsigned_resource(num_procs_per_node, "num_procs_per_node", false);
    parse_unsigned_resource(cores_per_proc, "cores_per_proc", false);
    parse_unsigned_resource(omp_num_threads, "omp_num_threads", false);
    parse_unsigned_resource(gpus_per_proc, "gpus_per_proc", true);
    if (nodes > ULONG_MAX / procs_per_node) {
        ERROR("Factory resource field total process count overflows");
        exit(EXIT_FAILURE);
    }
    const std::string total_procs = std::to_string(nodes * procs_per_node);

    if (scheduler == "none") {
        std::string command;
        if (launcher == "none") {
            command = target;
        } else if (launcher == "mpirun") {
            command = "OMP_NUM_THREADS=" + omp_num_threads + " mpirun ";
            command += "--prefix $(which mpirun | xargs dirname)/../ ";
            command += "-x OMP_NUM_THREADS -x PATH -x LD_LIBRARY_PATH ";
            command += "-np " + total_procs + " ";
            command += "--map-by ppr:" + num_procs_per_node + ":node ";
            command += "--map-by ppr:1:PE=" + cores_per_proc + " ";
            append_if_present(command, launcher_opts);
            command += target;
        } else if (launcher == "srun") {
            command = "OMP_NUM_THREADS=" + omp_num_threads + " srun ";
            command += "--ntasks=" + total_procs + " ";
            command += "--ntasks-per-node=" + num_procs_per_node + " ";
            command += "--cpus-per-task=" + cores_per_proc + " ";
            if (gpus_per_proc != "0") {
                command += "--gpus-per-task=" + gpus_per_proc + " ";
            }
            append_if_present(command, launcher_opts);
            command += target;
        } else {
            ERROR("Unsupported launcher: " + launcher);
            exit(EXIT_FAILURE);
        }
        command += " > kez.out 2> kez.err";
        return command;
    }

    if (scheduler == "slurm") {
        std::string command = "sbatch ";
        command += "--job-name=${PWD##*/} ";
        command += "--nodes=" + num_nodes + " ";
        command += "--ntasks-per-node=" + num_procs_per_node + " ";
        command += "--cpus-per-task=" + cores_per_proc + " ";
        if (gpus_per_proc != "0") {
            command += "--gpus-per-task=" + gpus_per_proc + " ";
        }
        append_if_present(command, scheduler_opts);
        command += "--output=kez.out --error=kez.err ";
        command += "--wrap=\"OMP_NUM_THREADS=" + omp_num_threads + " ";

        if (launcher == "none") {
            command += target + "\"";
        } else if (launcher == "mpirun") {
            command += "mpirun ";
            append_if_present(command, launcher_opts);
            command += target + "\"";
        } else if (launcher == "srun") {
            command += "srun ";
            append_if_present(command, launcher_opts);
            command += target + "\"";
        } else {
            ERROR("Unsupported launcher: " + launcher);
            exit(EXIT_FAILURE);
        }
        return command;
    }

    ERROR("Unsupported scheduler: " + scheduler);
    exit(EXIT_FAILURE);
}
