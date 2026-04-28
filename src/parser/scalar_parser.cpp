#include <parser/scalar_parser.hpp>

std::string parse_scalar(const std::string& scalar_str,
                         std::unordered_map<std::string, std::string>& template_map,
                         const YAML::Node& user_config, const YAML::Node& user_config_pkg,
                         const std::string& build_mode, const std::string& env_path) {
    auto resolver = [&](const std::string& template_name) {
        return parse_template(template_name, template_map, user_config, user_config_pkg, build_mode,
                              env_path);
    };
    return resolve_templates_in_scalar(scalar_str, resolver, "variable");
}
