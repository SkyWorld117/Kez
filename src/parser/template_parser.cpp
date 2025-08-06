#include "template_parser.h"

std::string parse_template(
    const std::string& template_str,
    std::unordered_map<std::string, std::string>& template_map,
    const YAML::Node& user_config,
    const YAML::Node& user_config_pkg,
    const std::string& build_mode,
    const std::string& env_path
) {
    // Check if the template is already resolved
    if (template_map.find(template_str) != template_map.end()) {
        // Check if the template is <package_name>.config.<option_name>
        if (template_str.find(".config.") != std::string::npos) {
            std::string value = template_map.at(template_str).substr(template_map.at(template_str).find('.') + 1);
            if (value.empty()) {
                ERROR("Template '" + template_str + "' is empty.");
                exit(EXIT_FAILURE);
            }
            return value;
        }
        return template_map.at(template_str);
    }

    // If not resolved, there can only be a few cases:
    // 1. context dependent template, such as `compiler.c` or `source`
    // 2. properties of any package in the dependency list, including the current package
    // 3. properties of abstract packages, need to redirect to the selected implementation
    // TODO: Write in the documentation that the template dependency within a package must be resolved from top to bottom.

    if (template_str == "source") {
        std::filesystem::path env_path_fs(env_path);
        std::filesystem::path source_path = env_path_fs / ".tmp" / "source";
        return source_path.string();
    }

    if (template_str.find("compiler.") == 0) {
        // Compiler templates are context dependent
        std::string compiler_name;
        std::string compiler_property = template_str.substr(9); // Remove "compiler."
        if (user_config_pkg["compiler"]) {
            std::string compiler_spec = user_config_pkg["compiler"].as<std::string>();
            if (compiler_spec == "system") {
                compiler_name = "gcc"; // Default to gcc for system compiler
            } else {
                compiler_name = compiler_spec.substr(0, compiler_spec.find('@')); // Extract compiler name before '@', format is "compiler@version"
            }
            if (compiler_property.find("config.") == 0 || compiler_property.find("env.") == 0) {
                ERROR("Compiler properties cannot be used in templates: " + template_str);
                exit(EXIT_FAILURE);
            }
            // Handle the special case of "prefix"
            // The reason for this is that a building environment may require multiple compilers, and the prefix is not always the same.
            // So the same name is mapped to different prefixes, thus we cannot use `template_map` to resolve it.
            // This requires using `${compiler.prefix}` in the template of the compiler configuration files.
            if (compiler_property == "prefix") {
                std::filesystem::path fromager_env(getenv("FROMAGER_ENV"));
                std::filesystem::path prefix_path;
                if (compiler_spec == "system") {
                    prefix_path = fromager_env / "system";
                } else {
                    prefix_path = fromager_env / "compilers" / (compiler_name + "-" + compiler_spec.substr(compiler_spec.find('@') + 1));
                }
                if (!std::filesystem::exists(prefix_path)) {
                    WARNING("Compiler prefix path does not exist: " + prefix_path.string());
                }
                return prefix_path.string();
            }
            // For other compiler properties, we can resolve them using the parse_property function
            return parse_property(
                compiler_name + "." + compiler_property,
                template_map,
                user_config,
                user_config_pkg,
                build_mode,
                env_path
            );
        } else {
            ERROR("Compiler specification not found in user config for package: " + template_str);
            exit(EXIT_FAILURE);
        }
    }

    std::vector<std::string> abstract_packages;
    for (const auto& item : user_config["recipe"]["abstract_packages"]) {
        std::string abstract_pkg_name = item.first.as<std::string>();
        abstract_packages.push_back(abstract_pkg_name);
    }

    std::string package_name = template_str.substr(0, template_str.find('.'));
    if (std::find(abstract_packages.begin(), abstract_packages.end(), package_name) != abstract_packages.end()) {
        // If the package is an abstract package, we need to resolve it
        std::string concrete_pkg_name = user_config["recipe"]["abstract_packages"][package_name].as<std::string>();
        std::string result = parse_property(
            concrete_pkg_name + "." + template_str.substr(package_name.length() + 1),
            template_map,
            user_config,
            user_config_pkg,
            build_mode,
            env_path
        );
        template_map[template_str] = result; // Cache the resolved template
        template_map[concrete_pkg_name + "." + template_str.substr(package_name.length() + 1)] = result; // Cache the concrete package template
        return result;
    } else {
        // If the package is not abstract, we can resolve it directly
        std::string result = parse_property(
            template_str,
            template_map,
            user_config,
            user_config_pkg,
            build_mode,
            env_path
        );
        template_map[template_str] = result; // Cache the resolved template
        return result;
    }
}
