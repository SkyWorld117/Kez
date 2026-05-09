#include <filesystem>
#include <parser/fromager_parser.hpp>
#include <utils/bash_utils.hpp>

std::string parse_fromager_template(const std::string& template_str) {
    if (template_str == "fromager.arch" || template_str.find("fromager.arch.") == 0) {
        return parse_fromager_arch(template_str);
    } else {
        ERROR("Unknown Fromager template '" + template_str + "'.");
        exit(EXIT_FAILURE);
    }
}

std::string parse_fromager_template_in_scalar(const std::string& command) {
    auto resolver = [&](const std::string& template_str) {
        if (template_str.empty() || template_str.find("fromager.") != 0) {
            ERROR("Invalid Fromager template. The correct format should be "
                  "'fromager.<template_name>'.");
            exit(EXIT_FAILURE);
        }
        if (template_str.find("fromager.arch") == 0) {
            return parse_fromager_arch(template_str);
        } else {
            ERROR("Unknown Fromager template '" + template_str + "'");
            exit(EXIT_FAILURE);
        }
    };
    // The prefix searched needs to match general ${ and we validate it in the resolver
    // but the original code specifically searched for ${fromager.
    // We can just use the generic one, and it will resolve *any* ${...} templates
    // that are fromager templates, which is correct for parse_fromager_template_in_scalar
    // Wait, if there are non-fromager templates, they would fail in the resolver.

    std::string result = command;
    size_t pos         = 0;
    while ((pos = result.find("${fromager.", pos)) != std::string::npos) {
        size_t end_pos = result.find('}', pos);
        if (end_pos == std::string::npos) {
            ERROR("Unclosed Fromager template in string: " + result);
            exit(EXIT_FAILURE);
        }
        std::string template_str      = result.substr(pos + 2, end_pos - pos - 2);
        std::string resolved_template = resolver(template_str);
        result.replace(pos, end_pos - pos + 1, resolved_template);
        // Do not add length since we might want to continue searching. But wait, it's standard replacement.
    }
    return result;
}

std::string parse_fromager_arch(const std::string& template_str) {
    std::string arch_variant = template_str.substr(13);  // Remove "fromager.arch"
    if (arch_variant.empty()) {
        arch_variant = "default";
    } else if (arch_variant[0] == '.') {
        arch_variant = arch_variant.substr(1);  // Remove the leading dot
    } else {
        ERROR("Invalid architecture template: " + template_str +
              ". The correct format should be 'fromager.arch' or 'fromager.arch.<arch_variant>'.");
        exit(EXIT_FAILURE);
    }

    std::filesystem::path fgr_home_path    = std::filesystem::path(get_env_var("FROMAGER_HOME"));
    std::filesystem::path arch_config_path = fgr_home_path / "heuristics" / "architecture.yaml";
    YAML::Node architecture_config         = YAML::LoadFile(arch_config_path.string());
    architecture_config                    = architecture_config["architecture"];
    std::string arch                       = get_env_var("FROMAGER_ARCH");

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