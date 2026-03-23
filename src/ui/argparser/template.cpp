#include <ui/argparser/argparser.hpp>

static argparse::ArgumentParser template_parser("template");
static argparse::ArgumentParser template_parse_parser("parse");

argparse::ArgumentParser& get_template_parser() {
    template_parser.add_description("Fetch a template for an application");

    template_parse_parser.add_description("Parse a user configuration file into instructions");
    template_parse_parser.add_argument("file");

    template_parser.add_argument("package").help("Package for template generation").nargs(1);
    template_parser.add_argument("-s", "--save").help("Save the configuration template").nargs(1);

    template_parser.add_subparser(template_parse_parser);

    return template_parser;
}

void execute_template_parser() {
    if (template_parser.is_subcommand_used("parse")) {
        std::string file  = template_parse_parser.get<std::string>("file");
        YAML::Node config = YAML::LoadFile(file);

        std::filesystem::path tmp_path = std::filesystem::path(getenv("FROMAGER_WORKDIR")) / ".tmp";
        std::filesystem::create_directories(tmp_path);

        YAML::Node instructions_yaml = parse(config, "release", tmp_path.string());
        YAML::Emitter out;
        out << instructions_yaml;
        std::ofstream ofs((tmp_path / "ins.yaml").string());
        if (!ofs) {
            ERROR("Failed to create instruction file");
            exit(EXIT_FAILURE);
        }
        ofs << out.c_str();
        ofs.close();

        SUCCESS("Instructions written to: " + (tmp_path / "ins.yaml").string());

        exit(EXIT_SUCCESS);
    } else {
        std::string package = template_parser.get<std::string>("package");
        bool save_template  = template_parser.is_used("--save");

        YAML::Node user_config = gen_user_config(package, save_template);
        YAML::Emitter out;
        out << user_config;

        std::cout << out.c_str() << std::endl;

        if (save_template) {
            std::string output_file = template_parser.get<std::string>("--save");
            std::ofstream ofs(output_file);
            if (!ofs) {
                ERROR("Could not open output file: " + output_file);
                exit(EXIT_FAILURE);
            }
            ofs << out.c_str();
            ofs.close();
            SUCCESS("Configuration template written to: " + output_file);
        } else {
            SUCCESS("Configuration template output to stdout.");
        }

        exit(EXIT_SUCCESS);
    }
}