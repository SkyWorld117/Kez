#include <sys/wait.h>

#include <algorithm>
#include <cstdlib>
#include <database/config.hpp>
#include <database/database.hpp>
#include <filesystem>
#include <iostream>
#include <parser/user_config_parser.hpp>
#include <string>
#include <ui/ui_utils.hpp>
#include <utils/bash_utils.hpp>
#include <utils/colored_io.hpp>
#include <utils/yaml_utils.hpp>

namespace {

    /**
     * @brief Resolves an abstract package name to a concrete target package.
     *
     * Looks up @p target in the user configuration's `recipe.abstract_packages` map.
     * If a mapping exists, the concrete package name is returned; otherwise the
     * original target is used unchanged.
     *
     * @param user_config  The parsed user configuration YAML node.
     * @param target       The (possibly abstract) package name to resolve.
     * @return The concrete target package name if found in `abstract_packages`,
     *         or @p target unchanged.
     */
    std::string effective_target(const YAML::Node& user_config, const std::string& target) {
        const YAML::Node recipe = user_config["recipe"];
        if (yaml_has(recipe, "abstract_packages") && recipe["abstract_packages"].IsMap() &&
            recipe["abstract_packages"][target].IsDefined()) {
            return yaml_scalar(recipe["abstract_packages"][target],
                               "abstract package selection for " + target);
        }
        return target;
    }

    std::string package_version(const YAML::Node& user_config, const std::string& package) {
        const YAML::Node kez = user_config["kez"];
        if (!kez.IsMap() || !kez[package].IsMap() || !yaml_has(kez[package], "version")) {
            ERROR("Invalid user configuration: package '" + package + "' has no version");
            exit(EXIT_FAILURE);
        }
        std::string version            = yaml_scalar(kez[package]["version"], package + ".version");
        const std::size_t local_source = version.find('@');
        if (local_source != std::string::npos) {
            version.erase(local_source);
        }
        validate_path_component(version, "package version");
        return version;
    }

    std::string package_compiler(const YAML::Node& user_config, const std::string& package) {
        const YAML::Node kez = user_config["kez"];
        if (!kez.IsMap() || !kez[package].IsMap() || !yaml_has(kez[package], "compiler")) {
            return "system";
        }
        std::string compiler = yaml_scalar(kez[package]["compiler"], package + ".compiler");
        std::replace(compiler.begin(), compiler.end(), '@', '-');
        validate_path_component(compiler, "compiler specification");
        return compiler;
    }

}  // namespace

std::string package_type_name(PackageType type) {
    switch (type) {
        case PackageType::Package: return "package";
        case PackageType::System: return "system";
        case PackageType::Compiler: return "compiler";
        case PackageType::Mpi: return "mpi";
        case PackageType::Vendor: return "vendor";
        case PackageType::Abstract: return "abstract";
        case PackageType::External: return "external";
    }
    return "unknown";
}

std::filesystem::path configured_work_path(const std::string& name) {
    const std::filesystem::path home = get_env_var("KEZ_HOME");
    const std::filesystem::path work = get_env_var("KEZ_WORKDIR");
    const YAML::Node manifest        = cached_yaml_load(home / "manifest.yaml");
    if (!yaml_has(manifest, "paths") || !manifest["paths"].IsMap() ||
        !yaml_has(manifest["paths"], name)) {
        ERROR("Invalid manifest: paths." + name + " is missing");
        exit(EXIT_FAILURE);
    }
    const std::filesystem::path configured = yaml_scalar(manifest["paths"][name], "paths." + name);
    return configured.is_absolute() ? configured : work / configured;
}

std::vector<std::string> user_config_targets(const YAML::Node& user_config) {
    if (!yaml_has(user_config, "recipe") || !user_config["recipe"].IsMap() ||
        !yaml_has(user_config["recipe"], "targets") ||
        !user_config["recipe"]["targets"].IsSequence()) {
        ERROR("Invalid user configuration: recipe.targets must be a sequence");
        exit(EXIT_FAILURE);
    }

    std::vector<std::string> result;
    for (const YAML::Node& target : user_config["recipe"]["targets"]) {
        result.push_back(yaml_scalar(target, "target package"));
    }
    if (result.empty()) {
        ERROR("Invalid user configuration: recipe.targets must not be empty");
        exit(EXIT_FAILURE);
    }
    return result;
}

std::filesystem::path installation_prefix(const YAML::Node& user_config,
                                          const std::string& environment_name, bool utilities) {
    const std::vector<std::string> targets = user_config_targets(user_config);
    PackageType common_type                = PackageType::Abstract;
    std::string selected_package;
    for (const std::string& requested : targets) {
        const std::string package = effective_target(user_config, requested);
        const PackageType type    = get_db_config(package)->type;
        if (selected_package.empty()) {
            selected_package = package;
            common_type      = type;
        } else if (common_type != type) {
            ERROR("All installation targets must have the same package type; '" + package +
                  "' is " + package_type_name(type) + " while previous targets are " +
                  package_type_name(common_type));
            exit(EXIT_FAILURE);
        }
    }

    if (utilities) {
        if (common_type != PackageType::Package) {
            ERROR("Only regular packages can be installed as utilities");
            exit(EXIT_FAILURE);
        }
        return configured_work_path("utilities");
    }

    if ((common_type == PackageType::Compiler || common_type == PackageType::Mpi ||
         common_type == PackageType::Vendor || common_type == PackageType::External) &&
        targets.size() != 1) {
        ERROR("Only one compiler, MPI, vendor, or external target can be installed at a time");
        exit(EXIT_FAILURE);
    }

    if (common_type == PackageType::System) {
        return configured_work_path("system");
    }
    if (common_type == PackageType::Compiler) {
        return configured_work_path("compilers") /
               (selected_package + "-" + package_version(user_config, selected_package));
    }
    if (common_type == PackageType::Mpi) {
        return configured_work_path("mpis") /
               (selected_package + "-" + package_version(user_config, selected_package) + "-" +
                package_compiler(user_config, selected_package));
    }
    if (common_type == PackageType::Vendor) {
        return configured_work_path("vendors") /
               (selected_package + "-" + package_version(user_config, selected_package));
    }
    if (common_type == PackageType::External) {
        ERROR("External packages are configured in config.yaml and cannot be installed");
        exit(EXIT_FAILURE);
    }
    if (common_type != PackageType::Package) {
        ERROR("Cannot install unresolved abstract package '" + selected_package + "'");
        exit(EXIT_FAILURE);
    }

    std::string selected_environment = environment_name;
    if (selected_environment.empty()) {
        selected_environment = get_env_var_noerr("KEZ_ACTIVE_ENV");
    }
    if (selected_environment.empty()) {
        ERROR("A target environment is required; pass --env <name> or run 'kez env enter <name>'");
        exit(EXIT_FAILURE);
    }
    validate_path_component(selected_environment, "environment name");
    return configured_work_path("applications") / selected_environment;
}

void validate_path_component(const std::string& value, const std::string& description) {
    const std::filesystem::path path(value);
    if (value.empty() || value == "." || value == ".." || path.has_parent_path() ||
        path.filename().string() != value) {
        ERROR("Invalid " + description + ": '" + value + "'");
        exit(EXIT_FAILURE);
    }
}

std::string required_name(const CommandArguments& arguments,
                          const std::string& action_description) {
    if (arguments.size() != 2) {
        ERROR(action_description + " requires exactly one name");
        exit(EXIT_FAILURE);
    }
    validate_path_component(arguments[1], action_description + " name");
    return arguments[1];
}

void run_external_command(const std::string& command) {
    const int status = std::system(command.c_str());
    if (status == -1 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        ERROR("Command failed: " + command);
        exit(EXIT_FAILURE);
    }
}

void list_directories(const std::filesystem::path& root, const std::string& heading) {
    if (!std::filesystem::is_directory(root)) {
        INFO("No " + heading + " found.");
        return;
    }
    std::vector<std::string> entries;
    for (const auto& entry : std::filesystem::directory_iterator(root)) {
        if (entry.is_directory()) {
            entries.push_back(entry.path().filename().string());
        }
    }
    std::sort(entries.begin(), entries.end());
    if (entries.empty()) {
        INFO("No " + heading + " found.");
        return;
    }
    INFO("Available " + heading + ":");
    for (const std::string& entry : entries) {
        std::cout << "  - " << entry << '\n';
    }
}

void emit_environment_activation(const std::filesystem::path& prefix, const std::string& variable,
                                 const std::string& value) {
    std::cout << "export PATH=" << shell_single_quote((prefix / "bin").string())
              << ":\"${PATH}\"; export " << variable << '=' << shell_single_quote(value) << '\n';
}

void emit_environment_deactivation(const std::filesystem::path& prefix,
                                   const std::string& variable) {
    std::cout << "kez_remove_path=" << shell_single_quote((prefix / "bin").string())
              << "; kez_new_path=''; kez_old_ifs=\"$IFS\"; "
                 "IFS=: read -r -a kez_path_parts <<< \"$PATH\"; IFS=\"$kez_old_ifs\"; "
                 "for kez_path_entry in \"${kez_path_parts[@]}\"; do "
                 "if [ \"$kez_path_entry\" != \"$kez_remove_path\" ]; then "
                 "if [ -z \"$kez_new_path\" ]; then kez_new_path=\"$kez_path_entry\"; "
                 "else kez_new_path=\"$kez_new_path:$kez_path_entry\"; fi; fi; done; "
                 "export PATH=\"$kez_new_path\"; unset "
              << variable
              << "; unset kez_remove_path kez_new_path kez_old_ifs kez_path_entry kez_path_parts\n";
}

void print_command_plan(const BashCommandPlan& plan) {
    for (const PackageCommands& package : plan) {
        INFO("Instructions for " + package.package + ":");
        for (const std::string& command : package.commands) {
            std::cout << " -  " << command << '\n';
        }
    }
}
