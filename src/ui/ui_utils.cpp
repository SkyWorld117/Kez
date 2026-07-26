#include <sys/wait.h>

#include <algorithm>
#include <cstdlib>
#include <database/config.hpp>
#include <database/database.hpp>
#include <filesystem>
#include <iostream>
#include <string>
#include <uconf_parser/user_config_parser.hpp>
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
        case PackageType::MPI: return "mpi";
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
                                          const std::string& environment_name, bool utilities,
                                          const std::string& renamed_version) {
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
        if (!renamed_version.empty()) {
            ERROR("--rename is not valid for utility installation");
            exit(EXIT_FAILURE);
        }
        if (common_type != PackageType::Package) {
            ERROR("Only regular packages can be installed as utilities");
            exit(EXIT_FAILURE);
        }
        return configured_work_path("utilities");
    }

    if ((common_type == PackageType::Compiler || common_type == PackageType::MPI ||
         common_type == PackageType::Vendor || common_type == PackageType::External) &&
        targets.size() != 1) {
        ERROR("Only one compiler, MPI, vendor, or external target can be installed at a time");
        exit(EXIT_FAILURE);
    }

    if (!renamed_version.empty()) {
        if (common_type != PackageType::Compiler && common_type != PackageType::MPI &&
            common_type != PackageType::Vendor) {
            ERROR("--rename is only valid for compiler, MPI, or vendor installations");
            exit(EXIT_FAILURE);
        }
        validate_path_component(renamed_version, "renamed version");
    }

    if (common_type == PackageType::System) {
        return configured_work_path("system");
    }
    if (common_type == PackageType::Compiler) {
        const std::string version = package_version(user_config, selected_package);
        return configured_work_path("compilers") /
               (selected_package + "-" + (renamed_version.empty() ? version : renamed_version));
    }
    if (common_type == PackageType::MPI) {
        const std::string version = package_version(user_config, selected_package);
        return configured_work_path("mpis") /
               (selected_package + "-" + (renamed_version.empty() ? version : renamed_version) +
                "-" + package_compiler(user_config, selected_package));
    }
    if (common_type == PackageType::Vendor) {
        const std::string version = package_version(user_config, selected_package);
        return configured_work_path("vendors") /
               (selected_package + "-" + (renamed_version.empty() ? version : renamed_version));
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
        ERROR(
            "A target environment is required; pass --env <name> or run 'kez env activate <name>'");
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
        INFO("  - " + entry);
    }
}

/**
 * @brief Collect the bin, man, and pkg-config directories under each
 *        package subdirectory of an environment root, mimicking the
 *        logic in gen_modulefile.sh.
 *
 * Hidden directories (names starting with '.') are skipped so that
 * internal directories like .tmp are not included.
 */
namespace {
    struct EnvironmentPaths {
        std::vector<std::string> path_dirs;
        std::vector<std::string> man_dirs;
        std::vector<std::string> pkgcfg_dirs;
        std::string virtual_environment;
    };

    EnvironmentPaths collect_environment_paths(const std::filesystem::path& prefix) {
        EnvironmentPaths result;
        if (!std::filesystem::is_directory(prefix)) {
            return result;
        }
        const std::filesystem::path virtual_environment = prefix / ".venv";
        if (std::filesystem::is_directory(virtual_environment / "bin")) {
            result.virtual_environment = virtual_environment.string();
            result.path_dirs.push_back((virtual_environment / "bin").string());
        }
        for (const auto& entry : std::filesystem::directory_iterator(prefix)) {
            if (!entry.is_directory()) continue;
            const std::string name = entry.path().filename().string();
            if (name.empty() || name[0] == '.') continue;

            const auto pkg_dir = entry.path();

            const auto bin_dir = pkg_dir / "bin";
            if (std::filesystem::is_directory(bin_dir)) {
                result.path_dirs.push_back(bin_dir.string());
            }

            const auto man_dir = pkg_dir / "share" / "man";
            if (std::filesystem::is_directory(man_dir)) {
                result.man_dirs.push_back(man_dir.string());
            }

            for (const char* rel : {"lib/pkgconfig", "lib64/pkgconfig", "share/pkgconfig"}) {
                const auto cfg_dir = pkg_dir / rel;
                if (std::filesystem::is_directory(cfg_dir)) {
                    result.pkgcfg_dirs.push_back(cfg_dir.string());
                }
            }
        }
        return result;
    }
}  // namespace

void emit_environment_activation(const std::filesystem::path& prefix, const std::string& variable,
                                 const std::string& value) {
    const EnvironmentPaths paths = collect_environment_paths(prefix);

    if (!paths.virtual_environment.empty()) {
        const std::string previous = variable + "_PREVIOUS_VIRTUAL_ENV";
        const std::string was_set  = previous + "_SET";
        std::cout << "if [[ ${VIRTUAL_ENV+x} ]]; then export " << previous
                  << "=\"${VIRTUAL_ENV}\"; export " << was_set << "=1; else unset " << previous
                  << "; export " << was_set
                  << "=0; fi; export VIRTUAL_ENV=" << shell_single_quote(paths.virtual_environment)
                  << "; ";
    }

    if (!paths.path_dirs.empty()) {
        std::cout << "export PATH=" << shell_single_quote(join(paths.path_dirs, ":"))
                  << ":\"${PATH}\"; ";
    }
    if (!paths.man_dirs.empty()) {
        std::cout << "export MANPATH=" << shell_single_quote(join(paths.man_dirs, ":"))
                  << ":\"${MANPATH}\"; ";
    }
    if (!paths.pkgcfg_dirs.empty()) {
        std::cout << "export PKG_CONFIG_PATH=" << shell_single_quote(join(paths.pkgcfg_dirs, ":"))
                  << ":\"${PKG_CONFIG_PATH}\"; ";
    }
    std::cout << "export " << variable << '=' << shell_single_quote(value) << '\n';
}

void emit_environment_deactivation(const std::filesystem::path& prefix,
                                   const std::string& variable) {
    EnvironmentPaths paths                         = collect_environment_paths(prefix);
    const std::string expected_virtual_environment = (prefix / ".venv").string();
    const std::string expected_virtual_bin         = (prefix / ".venv" / "bin").string();
    if (std::find(paths.path_dirs.begin(), paths.path_dirs.end(), expected_virtual_bin) ==
        paths.path_dirs.end()) {
        paths.path_dirs.push_back(expected_virtual_bin);
    }

    // Emit a reusable helper that filters entries matching any of the
    // given patterns out of a colon-separated variable.
    auto emit_remove = [](const std::string& var, const std::vector<std::string>& dirs) {
        if (dirs.empty()) return;
        std::cout << "kez_rm=";
        for (std::size_t i = 0; i < dirs.size(); ++i) {
            if (i > 0) std::cout << ':';
            std::cout << shell_single_quote(dirs[i]);
        }
        std::cout << "; kez_nw=''; kez_ifs=\"$IFS\"; IFS=:; "
                     "for kez_p in $"
                  << var
                  << "; do "
                     "case \":$kez_rm:\" in *\":$kez_p:\"*) ;; *) "
                     "kez_nw=\"${kez_nw:+$kez_nw:}$kez_p\"; esac; done; "
                     "IFS=\"$kez_ifs\"; "
                     "export "
                  << var
                  << "=\"$kez_nw\"; "
                     "unset kez_rm kez_nw kez_ifs kez_p; ";
    };

    emit_remove("PATH", paths.path_dirs);
    emit_remove("MANPATH", paths.man_dirs);
    emit_remove("PKG_CONFIG_PATH", paths.pkgcfg_dirs);

    {
        const std::string previous = variable + "_PREVIOUS_VIRTUAL_ENV";
        const std::string was_set  = previous + "_SET";
        std::cout << "if [[ ${" << was_set << ":-0} == 1 ]]; then export VIRTUAL_ENV=\"${"
                  << previous << "}\"; elif [[ ${VIRTUAL_ENV:-} == "
                  << shell_single_quote(expected_virtual_environment)
                  << " ]]; then unset VIRTUAL_ENV; fi; unset " << previous << ' ' << was_set
                  << "; ";
    }

    std::cout << "unset " << variable << '\n';
}

void print_command_plan(const BashCommandPlan& plan) {
    for (const PackageCommands& package : plan) {
        INFO("Instructions for " + package.package + ":");
        if (package.requires_python_environment) {
            std::cout << " -  activate the target Python virtual environment" << std::endl;
        }
        if (!package.python_distribution.empty()) {
            std::cout << " -  Python distribution: " + package.python_distribution << std::endl;
        }
        for (const std::string& command : package.commands) {
            std::cout << " -  " + command << std::endl;
        }
    }
}
