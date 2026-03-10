#include "scalar_parser.h"

std::string parse_scalar(const std::string& scalar_str,
                         std::unordered_map<std::string, std::string>& template_map,
                         const YAML::Node& user_config, const YAML::Node& user_config_pkg,
                         const std::string& build_mode, const std::string& env_path) {
    // There can be templates in the scalar string, so we need to parse it
    // using the template parser.

    std::string result = scalar_str;
    size_t pos         = 0;
    while ((pos = result.find("${", pos)) != std::string::npos) {
        size_t end_pos = result.find('}', pos);
        if (end_pos == std::string::npos) {
            ERROR("Unclosed variable template in string: " + result);
            exit(EXIT_FAILURE);
        }
        std::string template_name     = result.substr(pos + 2, end_pos - pos - 2);
        std::string resolved_template = parse_template(template_name, template_map, user_config,
                                                       user_config_pkg, build_mode, env_path);
        result.replace(pos, end_pos - pos + 1, resolved_template);
        pos += resolved_template.length();  // Move past the resolved template
    }

    // After resolving all templates, return the final scalar string
    return result;
}
