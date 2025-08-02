#include "template_parser.h"

std::string parse_template(
    const std::string& template_str,
    std::unordered_map<std::string, std::string>& template_map,
    const YAML::Node& user_config,
    const YAML::Node& user_config_pkg_context,
    const YAML::Node& pkg_config,
    const std::string& build_mode,
    const std::string& env_path
) {
    // Check if the template is already resolved
    if (template_map.find(template_str) != template_map.end()) {
        return template_map[template_str];
    }

    // If not resolved, there can only be a few cases:
    // 1. context dependent template, such as `compiler.c` or `source`
    // 2. properties of any package in the dependency list, including the current package
    // 3. properties of abstract packages, need to redirect to the selected implementation
}