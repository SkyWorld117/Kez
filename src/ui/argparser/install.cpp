#include <ui/argparser/argparser.hpp>

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