#include <fstream>
#include <ui/argparser/argparser.hpp>
#include <ui/bash_completion/utils.hpp>

static argparse::ArgumentParser template_parser("template");
static argparse::ArgumentParser template_parse_parser("parse");

argparse::ArgumentParser& get_template_parser() {
    template_parser.add_description("Fetch a template for an application, or parse a config");

    // We do not add the subparser to template_parser directly, because p-ranav/argparse
    // cannot distinguish between a positional argument and a subparser without strict separation.
    // Instead we use nargs::any and manually route the control flow.
    template_parser.add_argument("args")
        .help("For fetching: <package>\nFor parsing: parse <file>")
        .nargs(argparse::nargs_pattern::any);

    template_parser.add_argument("-s", "--save").help("Save the configuration template").nargs(1);

    return template_parser;
}

void execute_template_parser() {
    std::vector<std::string> args = template_parser.get<std::vector<std::string>>("args");

    if (!args.empty() && args[0] == "parse") {
        if (args.size() < 2) {
            ERROR("Missing file argument for 'parse' subcommand.");
            exit(EXIT_FAILURE);
        }
        std::string file  = args[1];
        YAML::Node config = YAML::LoadFile(file);

        std::string fgr_workdir        = std::getenv("FROMAGER_WORKDIR");
        std::filesystem::path tmp_path = std::filesystem::path(getenv("FROMAGER_WORKDIR")) / ".tmp";
        std::filesystem::create_directories(tmp_path);

        YAML::Node instructions_yaml = parse(config, "release", fgr_workdir);
        YAML::Emitter out;
        out << instructions_yaml;
        std::ofstream ofs((tmp_path / "ins.yaml").string());
        if (!ofs) {
            ERROR("Failed to create instruction file");
            exit(EXIT_FAILURE);
        }
        ofs << out.c_str();
        ofs << std::endl;
        ofs.close();

        SUCCESS("Instructions written to: " + (tmp_path / "ins.yaml").string());

        exit(EXIT_SUCCESS);
    } else {
        if (args.empty()) {
            ERROR("Missing package argument for template fetching.");
            exit(EXIT_FAILURE);
        }
        std::vector<std::string> packages = args;
        bool save_template                = template_parser.is_used("--save");

        YAML::Node user_config = gen_user_config(packages, save_template);
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

std::vector<std::string> get_template_suggestions(const int comp_cword,
                                                  const std::vector<std::string>& comp_words) {
    if (comp_cword == 2) {
        std::vector<std::string> suggestions       = {"--help", "-h", "parse", "-s", "--save"};
        std::vector<std::string> database_packages = get_database_suggestions();
        suggestions.insert(suggestions.end(), database_packages.begin(), database_packages.end());
        return suggestions;
    } else if (comp_cword == 3 && comp_words[2] == "parse") {
        std::vector<std::string> suggestions  = {"--help", "-h"};
        std::vector<std::string> config_files = get_filesystem_suggestions(comp_cword, comp_words);
        suggestions.insert(suggestions.end(), config_files.begin(), config_files.end());
        return suggestions;
    } else if (comp_cword >= 3) {
        std::vector<std::string> suggestions;

        if (exists_in(comp_words, "--save") || exists_in(comp_words, "-s")) {
            // If the last word is --save or -s, suggest config file names from the filesystem
            if (comp_words[comp_cword - 1] == "--save" || comp_words[comp_cword - 1] == "-s") {
                std::vector<std::string> config_files =
                    get_filesystem_suggestions(comp_cword, comp_words);
                suggestions.insert(suggestions.end(), config_files.begin(), config_files.end());
            } else {
                std::vector<std::string> database_packages = get_database_suggestions();
                suggestions.insert(suggestions.end(), database_packages.begin(),
                                   database_packages.end());
            }
        } else {
            // If --save is not already present, suggest package names from the database as well as the --save option
            std::vector<std::string> database_packages = get_database_suggestions();
            suggestions.insert(suggestions.end(), database_packages.begin(),
                               database_packages.end());
            suggestions.push_back("--save");
            suggestions.push_back("-s");
        }

        return suggestions;
    }

    return {};
}