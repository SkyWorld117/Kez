#include <ui/argparser/argparser.hpp>
#include <ui/bash_completion/utils.hpp>

#include "database/database.hpp"

static argparse::ArgumentParser utilities_parser("utilities");
static argparse::ArgumentParser util_add_parser("add");
static argparse::ArgumentParser util_empty_parser("empty");

argparse::ArgumentParser& get_utilities_parser() {
    utilities_parser.add_description("Manage utilities");

    util_add_parser.add_description("Add a utility to the utilities cellar");
    auto& util_mutex_group = util_add_parser.add_mutually_exclusive_group();
    util_mutex_group.add_argument("-r", "--read")
        .help("Read a configuration file")
        .default_value(false)
        .implicit_value(true);
    util_mutex_group.add_argument("utility").help("Utility name").nargs(1);
    util_add_parser.add_argument("-c", "--config")
        .help("Additional configuration options for the utility in the format <option>=<value>")
        .nargs(argparse::nargs_pattern::at_least_one);

    util_empty_parser.add_description("Empty the utilities cellar");

    utilities_parser.add_subparser(util_add_parser);
    utilities_parser.add_subparser(util_empty_parser);

    return utilities_parser;
}

void execute_utilities_parser() {
    if (utilities_parser.is_subcommand_used("add")) {
        bool is_config_file = util_add_parser.get<bool>("--read");
        std::string target  = util_add_parser.get<std::string>("utility");

        CellarPathQuery query;
        // Set the `pkg_name` in the query to trigger the correct parsing logic in `get_cellar_path`
        // This should prevent non-regular packages to be installed
        if (is_config_file) {
            // TODO: Remember to change this part once we figure out how to handle multiple target packages in a single user config file.
            YAML::Node config          = YAML::LoadFile(target);
            std::string target_package = config["recipe"]["dependencies"][0].as<std::string>();
            query.pkg_name             = target_package;

            bool version_set = false;
            if (util_add_parser.is_used("--config")) {
                std::vector<std::string> config_options =
                    util_add_parser.get<std::vector<std::string>>("--config");
                query.pkg_version =
                    get_version_from_cmdline_config(query.pkg_name, config_options, version_set);
            }
            if (!version_set) {
                query.pkg_version = config["cheese"][query.pkg_name]["version"].as<std::string>();
            }
        } else {
            query.pkg_name = target;

            bool version_set = false;
            if (util_add_parser.is_used("--config")) {
                std::vector<std::string> config_options =
                    util_add_parser.get<std::vector<std::string>>("--config");
                query.pkg_version =
                    get_version_from_cmdline_config(query.pkg_name, config_options, version_set);
            }
            if (!version_set) {
                YAML::Node pkg_config = get_db_config(query.pkg_name);
                query.pkg_version =
                    pkg_config["cheese"]["source"]["releases"][0]["version"].as<std::string>();
            }
        }
        query.cellar_name            = "utilities";
        std::string utilities_cellar = get_cellar_path(query);

        if (util_add_parser.is_used("--config")) {
            std::vector<std::string> config_options =
                util_add_parser.get<std::vector<std::string>>("--config");
            parse_cmdline(target, is_config_file, utilities_cellar, config_options);
        } else {
            parse_cmdline(target, is_config_file, utilities_cellar);
        }

        EXE_AND_CHECK("${FROMAGER_HOME}/bin/fromager_install " + utilities_cellar);

        exit(EXIT_SUCCESS);
    }

    if (utilities_parser.is_subcommand_used("empty")) {
        std::filesystem::path utilities_cellar = global_config::get_path("utilities");
        if (std::filesystem::exists(utilities_cellar)) {
            for (const auto& entry : std::filesystem::directory_iterator(utilities_cellar)) {
                std::filesystem::remove_all(entry.path());
            }
            SUCCESS("Utilities cellar has been emptied.");
        } else {
            WARNING("Utilities cellar does not exist. Nothing to empty.");
        }

        exit(EXIT_SUCCESS);
    }
}

std::vector<std::string> get_utilities_suggestions(const int comp_cword,
                                                   const std::vector<std::string>& comp_words) {
    if (comp_cword == 2) {
        std::vector<std::string> suggestions = {"add", "empty", "-h", "--help"};
        return suggestions;
    } else if (comp_cword >= 3 && comp_words[2] == "add") {
        std::vector<std::string> suggestions;

        if (comp_cword == 3) {
            suggestions = {"--help", "-h"};
        }

        bool suggest_read_option   = false;
        bool suggest_config_option = false;

        if (exists_in(comp_words, "--read") || exists_in(comp_words, "-r")) {
            // If the last word is --read or -r, suggest config file names from the filesystem
            if (comp_cword > 3 &&
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

        if (suggest_read_option) {
            suggestions.push_back("--read");
            suggestions.push_back("-r");

            // If --read is not already present, suggest utility names from the database as well as the --read option
            std::vector<std::string> database_packages = get_database_suggestions();
            suggestions.insert(suggestions.end(), database_packages.begin(),
                               database_packages.end());
        }

        if (suggest_config_option) {
            suggestions.push_back("--config");
            suggestions.push_back("-c");
        }

        return suggestions;
    }

    return {};
}