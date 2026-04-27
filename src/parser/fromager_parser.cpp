#include <parser/fromager_parser.hpp>

std::string parse_fromager_template(const std::string& command) {
    std::string result = command;
    size_t pos         = 0;
    while ((pos = result.find("${fromager.", pos)) != std::string::npos) {
        size_t end_pos = result.find('}', pos);
        if (end_pos == std::string::npos) {
            ERROR("Unclosed Fromager template in string: " + result);
            exit(EXIT_FAILURE);
        }
        std::string template_str = result.substr(
            pos + 2 + 9, end_pos - pos - 2 - 9);  // Extract the template name after "${fromager."
        if (template_str.empty()) {
            ERROR("Invalid Fromager template: " + result +
                  ". The correct format should be '${fromager.<template_name>}'.");
            exit(EXIT_FAILURE);
        }

        std::string resolved_template;

        if (template_str.find("arch") == 0) {
            resolved_template = parse_fromager_arch(template_str);
        } else {
            ERROR("Unknown Fromager template '" + template_str + "' in string: " + result);
            exit(EXIT_FAILURE);
        }

        result.replace(pos, end_pos - pos + 1, resolved_template);
    }

    return result;
}

std::string parse_fromager_arch(const std::string& template_str) {
    std::string arch_variant = template_str.substr(4);  // Remove "arch"
    if (arch_variant.empty()) {
        arch_variant = "default";
    } else if (arch_variant[0] == '.') {
        arch_variant = arch_variant.substr(1);  // Remove the leading dot
    } else {
        ERROR("Invalid architecture template: " + template_str +
              ". The correct format should be 'fromager.arch' or 'fromager.arch.<arch_variant>'.");
        exit(EXIT_FAILURE);
    }

    std::filesystem::path fgr_home_path    = std::filesystem::path(getenv("FROMAGER_HOME"));
    std::filesystem::path arch_config_path = fgr_home_path / "heuristics" / "architecture.yaml";
    YAML::Node architecture_config         = YAML::LoadFile(arch_config_path.string());
    architecture_config                    = architecture_config["architecture"];
    std::string arch                       = getenv("FROMAGER_ARCH");

    std::string arch_config;
    if (!architecture_config[arch_variant]) {
        WARNING("Architecture for package '" + arch_variant +
                "' is not specified in the configuration. Using default values. If this is not "
                "intended, please modify the \"architecture.yaml\" file.");
        arch_config = "default";
    } else {
        arch_config = arch_variant;
    }

    if (!architecture_config[arch_config][arch]) {
        ERROR("Illformed configuration: architecture '" + arch + "' for package '" + arch_variant +
              "' is missing in the configuration file " + arch_config_path.string());
        exit(EXIT_FAILURE);
    }
    return architecture_config[arch_config][arch].as<std::string>();
}