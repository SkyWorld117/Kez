#include <ui/argparser/argparser.hpp>
#include <ui/bash_completion/utils.hpp>

static argparse::ArgumentParser install_parser("install");

argparse::ArgumentParser& get_install_parser() {
    install_parser.add_description("Install a package");
    install_parser.add_argument("-r", "--read")
        .help("Read a configuration file for installation")
        .default_value(false)
        .implicit_value(true);
    install_parser.add_argument("pkg_or_file")
        .help("Package name or config file if -r is used")
        .nargs(1);
    install_parser.add_argument("--config")
        .help("Configs to install with")
        .nargs(argparse::nargs_pattern::at_least_one);
    install_parser.add_argument("--cellar")
        .help("Target cellar (only used when installing a package that is not a "
              "compiler/MPI/vendor type)")
        .nargs(1);

    return install_parser;
}

void execute_install_parser() {
    bool is_config_file = install_parser.get<bool>("--read");
    std::string target  = install_parser.get<std::string>("pkg_or_file");

    CellarPathQuery query;
    if (is_config_file) {
        YAML::Node config          = YAML::LoadFile(target);
        std::string target_package = config["recipe"]["dependencies"][0].as<std::string>();
        query.pkg_name             = target_package;
    } else {
        query.pkg_name = target;
    }
    if (install_parser.is_used("--cellar")) {
        query.cellar_name = install_parser.get<std::string>("--cellar");
    }
    std::string target_cellar = get_cellar_path(query);

    if (install_parser.is_used("--config")) {
        std::vector<std::string> config_options =
            install_parser.get<std::vector<std::string>>("--config");
        parse_cmdline(target, is_config_file, target_cellar, config_options);
    } else {
        parse_cmdline(target, is_config_file, target_cellar);
    }

    EXE_AND_CHECK("${FROMAGER_HOME}/bin/fromager_install " + target_cellar);

    exit(EXIT_SUCCESS);
}

std::vector<std::string> get_install_suggestions(const int comp_cword,
                                                 const std::vector<std::string>& comp_words) {
    std::vector<std::string> suggestions;
    if (comp_cword == 2) {
        suggestions = {"--help", "-h"};
    }

    if (exists_in(comp_words, "--read") || exists_in(comp_words, "-r")) {
        // If the last word is --read or -r, suggest config file names from the filesystem
        if (comp_cword > 2 &&
            (comp_words[comp_cword - 1] == "--read" || comp_words[comp_cword - 1] == "-r")) {
            suggestions = get_filesystem_suggestions(comp_cword, comp_words);
        } else if (!exists_in(comp_words, "--config") && !exists_in(comp_words, "-c")) {
            // If --config is not already present, suggest it instead
            suggestions.push_back("--config");
            suggestions.push_back("-c");
        }
    } else {
        // If --read is not already present, suggest utility names from the database as well as the --read option
        std::vector<std::string> database_packages = get_database_suggestions();
        suggestions.insert(suggestions.end(), database_packages.begin(), database_packages.end());
        suggestions.push_back("--read");
        suggestions.push_back("-r");
    }

    return suggestions;
}