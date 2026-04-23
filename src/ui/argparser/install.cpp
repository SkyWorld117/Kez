#include <global_config.hpp>
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
        .nargs(argparse::nargs_pattern::at_least_one);
    install_parser.add_argument("-c", "--config")
        .help("Configs to install with")
        .nargs(argparse::nargs_pattern::at_least_one);
    install_parser.add_argument("-C", "--cellar")
        .help("Target cellar (only used when installing a package that is not a "
              "compiler/MPI/vendor type)")
        .nargs(1);
    install_parser.add_argument("-f", "--force")
        .help("Force reinstallation even if the package is already installed")
        .default_value(false)
        .implicit_value(true);

    return install_parser;
}

void execute_install_parser() {
    bool is_config_file = install_parser.get<bool>("--read");
    std::vector<std::string> pkg_or_file =
        install_parser.get<std::vector<std::string>>("pkg_or_file");

    CellarPathQuery query;
    if (is_config_file) {
        std::string target = pkg_or_file[0];
        YAML::Node config  = YAML::LoadFile(target);
        std::vector<std::string> target_packages =
            config["recipe"]["targets"].as<std::vector<std::string>>();
        query.pkg_name = target_packages;
    } else {
        query.pkg_name = pkg_or_file;
    }
    std::string pkg_type = get_pkg_type(query.pkg_name);

    if (pkg_type == "compiler" || pkg_type == "mpi" || pkg_type == "vendor") {
        bool version_set = false;
        if (install_parser.is_used("--config")) {
            std::vector<std::string> config_options =
                install_parser.get<std::vector<std::string>>("--config");
            query.pkg_version =
                get_version_from_cmdline_config(query.pkg_name[0], config_options, version_set);
        }

        if (!version_set) {
            if (is_config_file) {
                YAML::Node config = YAML::LoadFile(pkg_or_file[0]);
                query.pkg_version =
                    config["cheese"][query.pkg_name[0]]["version"].as<std::string>();
            } else {
                YAML::Node pkg_config = get_db_config(query.pkg_name[0]);
                query.pkg_version =
                    pkg_config["cheese"]["source"]["releases"][0]["version"].as<std::string>();
            }
        }

        if (pkg_type == "mpi") {
            bool compiler_spec_set = false;
            if (install_parser.is_used("--config")) {
                std::vector<std::string> config_options =
                    install_parser.get<std::vector<std::string>>("--config");
                query.compiler_spec = get_compiler_spec_from_cmdline_config(
                    query.pkg_name[0], config_options, compiler_spec_set);
            }

            if (!compiler_spec_set) {
                if (is_config_file) {
                    YAML::Node config = YAML::LoadFile(pkg_or_file[0]);
                    if (config["cheese"][query.pkg_name[0]]["compiler"]) {
                        query.compiler_spec =
                            config["cheese"][query.pkg_name[0]]["compiler"].as<std::string>();
                    } else {
                        query.compiler_spec = global_config::get_default_compiler();
                    }
                } else {
                    query.compiler_spec = global_config::get_default_compiler();
                }
            }
        }
    }

    if (install_parser.is_used("--cellar")) {
        query.cellar_name = install_parser.get<std::string>("--cellar");
    }
    std::string target_cellar = get_cellar_path(query);

    std::vector<std::string> config_options;
    if (install_parser.is_used("--config")) {
        config_options = install_parser.get<std::vector<std::string>>("--config");
    }
    if (is_config_file) {
        parse_cmdline(pkg_or_file[0], target_cellar, config_options);
    } else {
        parse_cmdline(pkg_or_file, target_cellar, config_options);
    }

    if (install_parser.get<bool>("--force")) {
        target_cellar += " --force";
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

    bool suggest_read_option   = false;
    bool suggest_config_option = false;
    bool suggest_cellar_option = false;

    if (exists_in(comp_words, "--read") || exists_in(comp_words, "-r")) {
        // If the last word is --read or -r, suggest config file names from the filesystem
        if (comp_cword > 2 &&
            (comp_words[comp_cword - 1] == "--read" || comp_words[comp_cword - 1] == "-r")) {
            suggestions = get_filesystem_suggestions(comp_cword, comp_words);
            return suggestions;
        }
    } else {
        suggest_read_option = true;
    }

    if (exists_in(comp_words, "--config") || exists_in(comp_words, "-c")) {
        if (comp_cword > 2 &&
            (comp_words[comp_cword - 1] == "--config" || comp_words[comp_cword - 1] == "-c")) {
            // TODO: Smart suggestions based on the config options already present in the command line
            // Do nothing for now
            return suggestions;
        }
    } else {
        suggest_config_option = true;
    }

    if (exists_in(comp_words, "--cellar") || exists_in(comp_words, "-C")) {
        // If --cellar is already present, suggest cellar names from the filesystem
        if (comp_cword > 2 &&
            (comp_words[comp_cword - 1] == "--cellar" || comp_words[comp_cword - 1] == "-C")) {
            std::vector<std::string> cellars = get_cellars_suggestions();
            suggestions.insert(suggestions.end(), cellars.begin(), cellars.end());
            return suggestions;
        }
    } else {
        suggest_cellar_option = true;
    }

    if (!exists_in(comp_words, "--force") && !exists_in(comp_words, "-f")) {
        suggestions.push_back("--force");
        suggestions.push_back("-f");
    }

    if (suggest_read_option) {
        suggestions.push_back("--read");
        suggestions.push_back("-r");

        // If --read is not already present, suggest utility names from the database as well as the --read option
        std::vector<std::string> database_packages = get_database_suggestions();
        suggestions.insert(suggestions.end(), database_packages.begin(), database_packages.end());
    }

    if (suggest_config_option) {
        suggestions.push_back("--config");
        suggestions.push_back("-c");
    }

    if (suggest_cellar_option) {
        suggestions.push_back("--cellar");
        suggestions.push_back("-C");
    }

    return suggestions;
}