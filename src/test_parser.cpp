#include "parser/broadcast.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <YAML file>" << std::endl;
        return 1;
    }

    std::filesystem::path yaml_file(argv[1]);
    if (!std::filesystem::exists(yaml_file)) {
        std::cerr << "File does not exist: " << yaml_file.string() << std::endl;
        return 1;
    }

    YAML::Node config = YAML::LoadFile(yaml_file.string());
    std::vector<std::string> templates = get_templates(config);

    if (templates.empty()) {
        std::cout << "No templates found in the YAML file." << std::endl;
    } else {
        std::cout << "Templates found:" << std::endl;
        for (const auto& tmpl : templates) {
            std::cout << "- " << tmpl << std::endl;
        }
    }

    // Broadcast the templates
    std::unordered_map<std::string, std::string> broadcasted_templates = broadcast(config);
    if (broadcasted_templates.empty()) {
        std::cout << "No templates to broadcast." << std::endl;
    } else {
        std::cout << "Broadcasted templates:" << std::endl;
        for (const auto& pair : broadcasted_templates) {
            std::cout << pair.first << " -> " << pair.second << std::endl;
        }
    }

    return 0;
}