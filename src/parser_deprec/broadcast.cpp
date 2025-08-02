#include "broadcast.h"

// Slightly modified version of the `templates_verifier`
std::vector<std::string> single_pkg_templates(const YAML::Node& node) {
    std::vector<std::string> templates;
    if (node.IsNull()) {
        return templates;
    }

    if (node.IsMap()) {
        for (const auto& item : node) {
            std::vector<std::string> first_templates = single_pkg_templates(item.first);
            std::vector<std::string> second_templates = single_pkg_templates(item.second);
            if (!first_templates.empty()) {
                templates.insert(templates.end(), first_templates.begin(), first_templates.end());
            }
            if (!second_templates.empty()) {
                templates.insert(templates.end(), second_templates.begin(), second_templates.end());
            }
        }
    } else if (node.IsSequence()) {
        for (const auto& item : node) {
            std::vector<std::string> item_templates = single_pkg_templates(item);
            templates.insert(templates.end(), item_templates.begin(), item_templates.end());
        }
    } else if (node.IsScalar()) {
        std::string str = node.as<std::string>();
        size_t pos = 0;
        while ((pos = str.find("${", pos)) != std::string::npos) {
            size_t end_pos;
            for (end_pos = pos + 2; end_pos != std::string::npos; end_pos++) {
                if (str[end_pos] == '{') {
                    ERROR("Error at line " + std::to_string(node.Mark().line) + ": " + str + "\n"
                          "An unclosed variable template starting at position " + std::to_string(pos) +
                          " or a nested template at position " + std::to_string(end_pos) + " was found.");
                    exit(EXIT_FAILURE);
                }
                if (str[end_pos] == '}') {
                    break;
                }
            }
            if (end_pos == std::string::npos) {
                ERROR("Unclosed variable template in string at line " + std::to_string(node.Mark().line) 
                      + ": " + str);
                exit(EXIT_FAILURE);
            }
            std::string var_name = str.substr(pos + 2, end_pos - pos - 2);
            if (var_name.empty()) {
                ERROR("Empty variable template ${} in string at line " + std::to_string(node.Mark().line) 
                      + ": " + str);
                exit(EXIT_FAILURE);
            }
            // Skip if the variable name starts with `compiler.` or is `source`
            if (var_name.rfind("compiler.", 0) != 0 && var_name != "source") {
                templates.push_back(var_name);
            }
            pos = end_pos + 1; // Move past the closing brace
        }
    }
    return templates;
}

// Use the user configuration file as index to fetch all the templates from dependent packages.
std::vector<std::string> get_templates(const YAML::Node& config) {
    std::filesystem::path db_path(getenv("CHEESE_DB"));

    std::vector<std::string> templates;
    for (const auto& item : config["cheese"]) {
        std::string pkg_name = item.first.as<std::string>();
        std::filesystem::path config_file(pkg_name + ".yaml");
        std::filesystem::path config_path = db_path / config_file;
        if (!std::filesystem::exists(config_path)) {
            ERROR("Configuration file does not exist: " + config_path.string());
            exit(EXIT_FAILURE);
        }

        YAML::Node pkg_config = YAML::LoadFile(config_path.string());
        std::vector<std::string> pkg_templates = single_pkg_templates(pkg_config);
        templates.insert(templates.end(), pkg_templates.begin(), pkg_templates.end());
    }

    // Remove duplicates
    std::sort(templates.begin(), templates.end());
    templates.erase(std::unique(templates.begin(), templates.end()), templates.end());

    // Remove templates of packages that are not in the full dependency list (or abstract packages)
    std::vector<std::string> abstract_packages;
    for (const auto& item : config["recipe"]["abstract_packages"]) {
        abstract_packages.push_back(item.first.as<std::string>());
    }
    std::vector<std::string> full_dependency_list = config["recipe"]["dependencies"].as<std::vector<std::string>>();
    std::vector<std::string> filtered_templates;
    for (const auto& tmpl : templates) {
        std::string pkg_name = tmpl.substr(0, tmpl.find('.'));
        if (std::find(full_dependency_list.begin(), full_dependency_list.end(), pkg_name) != full_dependency_list.end() ||
            std::find(abstract_packages.begin(), abstract_packages.end(), pkg_name) != abstract_packages.end()) {
            filtered_templates.push_back(tmpl);
        }
    }

    return filtered_templates;
}

// Read a user configuration file and broadcast the mapping for template evaluation.
std::unordered_map<std::string, std::string> broadcast(const YAML::Node& config) {
    std::vector<std::string> templates = get_templates(config);
    std::unordered_map<std::string, std::string> template_map;

    std::vector<std::string> abstract_packages;
    for (const auto& item : config["recipe"]["abstract_packages"]) {
        abstract_packages.push_back(item.first.as<std::string>());
    }

    std::vector<std::string> full_dependency_list = config["recipe"]["dependencies"].as<std::vector<std::string>>();

    // 1. Solve abstract linking: `<abstract_package_name>.use-<concrete_package_name>` to `true`
    // if it is in the recipe, and `false` otherwise.
    for (const auto& tmpl : templates) {
        std::string pkg_name = tmpl.substr(0, tmpl.find('.'));
        std::string attr = tmpl.substr(tmpl.find('.') + 1);
        if (std::find(abstract_packages.begin(), abstract_packages.end(), pkg_name) != abstract_packages.end() &&
            attr.rfind("use-", 0) == 0) {
            std::string concrete_pkg_name = attr.substr(4);
            std::string selected_concrete_pkg_name = config["recipe"]["abstract_packages"][pkg_name].as<std::string>();
            if (concrete_pkg_name == selected_concrete_pkg_name) {
                template_map[tmpl] = "true"; // Use the selected concrete package
            } else {
                template_map[tmpl] = "false"; // Use a different concrete package
            }
        }
    }

    for (const auto& tmpl : templates) {
        resolve_template(config, tmpl, template_map);
    }

    return template_map;
}