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
    /** @brief Holds the parsed commands and summary-regex pattern for a single profile run. */
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

    /** @brief Replace every occurrence of @p pattern in @p value with @p replacement. */
    void replace_all(std::string& value, const std::string& pattern,
                     const std::string& replacement) {
        std::size_t position = 0;
        while ((position = value.find(pattern, position)) != std::string::npos) {
            value.replace(position, pattern.size(), replacement);
            position += replacement.size();
        }
    }

    /** @brief Inherit a scalar value from the factory/buildspace/profile hierarchy,
     *         expanding ${key} references with the parent value. */
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

    /** @brief Extract the "factory" sub-map from @p config, or return @p config itself. */
    YAML::Node factory_body(const YAML::Node& config) {
        if (yaml_has(config, "factory")) {
            return config["factory"];
        }
        return config;
    }

    /** @brief Find a map element with the given @p name in a YAML sequence of maps. */
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

    /** @brief Terminate if @p name already appears in @p stack (circular sibling reference). */
    void reject_cycle(const std::vector<std::string>& stack, const std::string& name,
                      const std::string& description) {
        if (std::find(stack.begin(), stack.end(), name) != stack.end()) {
            ERROR(description + " sibling cycle includes '" + name + "'");
            exit(EXIT_FAILURE);
        }
    }

    /** @brief Parse @p value as a non-negative (or positive) unsigned long integer resource field. */
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

    /** @brief Like inherited_scalar but falls back to @p default_value when the result is empty. */
    std::string inherited_resource(const YAML::Node& factory_config,
                                   const YAML::Node& buildspace_config,
                                   const YAML::Node& profile_config, const std::string& key,
                                   const std::string& default_value) {
        std::string value =
            inherited_scalar(factory_config, buildspace_config, profile_config, key);
        return value.empty() ? default_value : value;
    }

    /** @brief Build a cp command that copies the contents of @p inputs into the current directory. */
    std::string copy_inputs_command(const std::string& inputs) {
        return "cp -a " + inputs + "/. .";
    }

    /** @brief Parse a single profile entry into a FactoryRun (commands + summary regex). */
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

    /** @brief Parse all profiles under a single buildspace entry. */
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

    /** @brief Append @p option and a trailing space to @p command if @p option is not empty. */
    void append_if_present(std::string& command, const std::string& option) {
        if (!option.empty()) {
            command += option + " ";
        }
    }
}  // namespace

/** @brief Parse the top-level factory configuration into a list of buildspace entries (each with profiles). */
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

/** @brief Build the shell command that launches @p target under the given launcher/scheduler/resource constraints. */
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
