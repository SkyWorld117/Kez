#include <ui/argparser/argparser.hpp>

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
    util_add_parser.add_argument("--config")
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
        } else {
            query.pkg_name = target;
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