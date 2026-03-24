#include <ui/argparser/argparser.hpp>

static argparse::ArgumentParser init_parser("init");

argparse::ArgumentParser& get_init_parser() {
    init_parser.add_description("Initialize the Fromager environment");
    init_parser.add_argument("--refresh")
        .help("Refresh the Fromager environment by reinstalling core utilities")
        .default_value(false)
        .implicit_value(true);

    return init_parser;
}

void execute_init_parser() {
    if (init_parser.get<bool>("--refresh")) {
        INFO("Refreshing Fromager system environment...");
        WARNING("Note: This will delete the existing system environment and recreate it.");
        EXE_AND_CHECK("${FROMAGER_HOME}/bin/fromager_init --refresh");
    } else {
        INFO("Initializing Fromager system environment...");
        EXE_AND_CHECK("${FROMAGER_HOME}/bin/fromager_init");
    }

    exit(EXIT_SUCCESS);
}

std::vector<std::string> get_init_suggestions(const int comp_cword,
                                              const std::vector<std::string>& comp_words) {
    if (comp_cword == 2) {
        std::vector<std::string> suggestions = {"--refresh", "-h", "--help"};
        return suggestions;
    }
    return {};
}