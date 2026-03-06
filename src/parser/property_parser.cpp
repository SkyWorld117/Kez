#include "property_parser.h"

std::string parse_property(const std::string&                            property_name,
                           std::unordered_map<std::string, std::string>& template_map,
                           const YAML::Node& user_config, const YAML::Node& user_config_pkg,
                           const std::string& build_mode, const std::string& env_path) {
    std::string package_name = property_name.substr(0, property_name.find('.'));
    std::string property     = property_name.substr(property_name.find('.') + 1);

    // Load the package configuration
    YAML::Node pkg_config_node = get_db_config(package_name);

    // Check if the package has the property
    if (property != "prefix" && property != "version" &&
        !pkg_config_node["cheese"]["properties"][property]) {
        ERROR("Property '" + property + "' not found in package '" + package_name + "'.");
        exit(EXIT_FAILURE);
    }

    if (property == "prefix") {
        if (pkg_config_node["cheese"]["type"].as<std::string>() == "mpi") {
            // For MPI packages, we need to handle the prefix differently
            // The goal is to share MPI libraries across the environments
            std::string mpi_version =
                user_config["cheese"][package_name]["version"].as<std::string>();
            std::string compiler_spec =
                user_config["cheese"][package_name]["compiler"].as<std::string>();
            std::filesystem::path fromager_env(getenv("FROMAGER_ENV"));
            std::filesystem::path prefix_path;
            if (compiler_spec == "system") {
                prefix_path =
                    fromager_env / "mpis" / (package_name + "-" + mpi_version + "-system");
            } else {
                std::string compiler_name    = compiler_spec.substr(0, compiler_spec.find('@'));
                std::string compiler_version = compiler_spec.substr(compiler_spec.find('@') + 1);
                prefix_path                  = fromager_env / "mpis" /
                              (package_name + "-" + mpi_version + "-" + compiler_name + "-" +
                               compiler_version);
            }
            if (!std::filesystem::exists(prefix_path)) {
                WARNING("MPI prefix path does not exist: " + prefix_path.string());
            }
            return prefix_path.string();
        } else if (pkg_config_node["cheese"]["type"].as<std::string>() == "system") {
            // For system packages, we can use the FROMAGER_ENV variable
            std::filesystem::path fromager_env(getenv("FROMAGER_ENV"));
            std::filesystem::path prefix_path = fromager_env / "system";
            return prefix_path.string();
        } else if (pkg_config_node["cheese"]["type"].as<std::string>() == "vendor") {
            // Some vendor packages are submodules of other vendor packages
            // We need to handle the prefix differently
            if (pkg_config_node["cheese"]["properties"] &&
                pkg_config_node["cheese"]["properties"]["prefix"]) {
                std::cout << "Using user-defined prefix for vendor package: " << package_name
                          << std::endl;
                return parse_scalar(
                    pkg_config_node["cheese"]["properties"]["prefix"].as<std::string>(),
                    template_map, user_config, user_config_pkg, build_mode, env_path);
            }
            // We share vendor packages across the environments
            std::string pkg_version =
                user_config["cheese"][package_name]["version"].as<std::string>();
            std::filesystem::path fromager_env(getenv("FROMAGER_ENV"));
            std::filesystem::path vendor_prefix_path =
                fromager_env / "vendors" / (package_name + "-" + pkg_version);
            return vendor_prefix_path.string();
        } else if (pkg_config_node["cheese"]["type"].as<std::string>() == "external") {
            // Information about external packages is stored in the `config.yaml` file
            std::filesystem::path fromager_workdir(getenv("FROMAGER_WORKDIR"));
            std::filesystem::path config_path = fromager_workdir / "config.yaml";
            YAML::Node            config_node = YAML::LoadFile(config_path.string());
            if (config_node["fromager"]["external"] &&
                config_node["fromager"]["external"][package_name]) {
                if (!config_node["fromager"]["external"][package_name]["prefix"].IsNull()) {
                    return config_node["fromager"]["external"][package_name]["prefix"]
                        .as<std::string>();
                } else {
                    ERROR("External package '" + package_name +
                          "' does not have a prefix defined in config.yaml.");
                    exit(EXIT_FAILURE);
                }
            } else {
                ERROR("External package '" + package_name + "' not found in config.yaml.");
                exit(EXIT_FAILURE);
            }
        }

        // Regular packages
        if (build_mode == "release") {
            return env_path;
        } else if (build_mode == "debug") {
            // Get the first key in the user_config["cheese"] map
            std::string target_package = user_config["cheese"].begin()->first.as<std::string>();
            return env_path + "/" + package_name;
        } else {
            ERROR("Unknown build mode: " + build_mode);
            exit(EXIT_FAILURE);
        }
    } else if (property == "version") {
        if (pkg_config_node["cheese"]["type"].as<std::string>() == "system") {
            std::filesystem::path fromager_env(getenv("FROMAGER_ENV"));
            std::filesystem::path state_path = fromager_env / "system" / "state.yaml";
            YAML::Node            state_node = YAML::LoadFile(state_path.string());
            if (state_node["cheese"] && state_node["cheese"][package_name] &&
                state_node["cheese"][package_name]["version"]) {
                return state_node["cheese"][package_name]["version"].as<std::string>();
            } else {
                ERROR("Package '" + package_name + "' version not found in state.yaml.");
                exit(EXIT_FAILURE);
            }
        } else if (pkg_config_node["cheese"]["type"].as<std::string>() == "external") {
            // For external packages, we can use the FROMAGER_ENV variable
            std::filesystem::path fromager_workdir(getenv("FROMAGER_WORKDIR"));
            std::filesystem::path config_path = fromager_workdir / "config.yaml";
            YAML::Node            config_node = YAML::LoadFile(config_path.string());
            if (config_node["fromager"]["external"] &&
                config_node["fromager"]["external"][package_name] &&
                config_node["fromager"]["external"][package_name]["version"]) {
                return config_node["fromager"]["external"][package_name]["version"]
                    .as<std::string>();
            } else {
                ERROR("External package '" + package_name + "' version not found in config.yaml.");
                exit(EXIT_FAILURE);
            }
        } else {
            if (user_config["cheese"][package_name]["version"]) {
                return user_config["cheese"][package_name]["version"].as<std::string>();
            } else {
                ERROR("Package '" + package_name + "' version not found in user config.");
                exit(EXIT_FAILURE);
            }
        }
    } else if (property == "c" or property == "cxx" or property == "fort" or
               property == "omp_flags") {
        std::string result = pkg_config_node["cheese"]["properties"][property].as<std::string>();
        std::string resolved_value =
            parse_scalar(result, template_map, user_config, user_config_pkg, build_mode, env_path);
        template_map[property_name] = resolved_value;
        return resolved_value;
    } else {
        // The rest of the properties needs to be solved in a second pass
        // due to possible dependencies on other properties
        return "${" + property_name + "}";
    }
}

std::string parse_complex_property(const std::string&                            template_str,
                                   std::unordered_map<std::string, std::string>& template_map,
                                   const YAML::Node& user_config, const YAML::Node& user_config_pkg,
                                   const std::string& build_mode, const std::string& env_path) {
    // Handle the special cases
    std::string package_name  = template_str.substr(0, template_str.find('.'));
    std::string property_name = template_str.substr(template_str.find('.') + 1);

    YAML::Node pkg_config_node = get_db_config(package_name);

    std::string base_value =
        pkg_config_node["cheese"]["properties"][property_name]["default"]
            ? pkg_config_node["cheese"]["properties"][property_name]["default"].as<std::string>()
            : "";
    std::string final_value;
    if (pkg_config_node["cheese"]["properties"][property_name]["conditions"]) {
        final_value = parse_conditions(
            base_value, pkg_config_node["cheese"]["properties"][property_name]["conditions"],
            template_map, user_config, pkg_config_node, build_mode, env_path);
    } else {
        final_value = base_value;
    }
    if (!final_value.empty()) {
        // Resolve templates in the value
        final_value = parse_scalar(final_value, template_map, user_config, user_config_pkg,
                                   build_mode, env_path);
    }
    template_map[template_str] = final_value;  // Cache the resolved template
    return final_value;
}

std::string parse_properties_in_scalar(const std::string&                            command,
                                       std::unordered_map<std::string, std::string>& template_map,
                                       const YAML::Node&                             user_config,
                                       const YAML::Node&  user_config_pkg,
                                       const std::string& build_mode, const std::string& env_path) {
    std::string result = command;
    size_t      pos    = 0;
    while ((pos = result.find("${", pos)) != std::string::npos) {
        size_t end_pos = result.find('}', pos);
        if (end_pos == std::string::npos) {
            ERROR("Unclosed property template in string: " + result);
            exit(EXIT_FAILURE);
        }
        std::string property_name     = result.substr(pos + 2, end_pos - pos - 2);
        std::string resolved_property = parse_complex_property(
            property_name, template_map, user_config, user_config_pkg, build_mode, env_path);
        result.replace(pos, end_pos - pos + 1, resolved_property);
        pos += resolved_property.length();  // Move past the resolved property
    }

    return result;
}