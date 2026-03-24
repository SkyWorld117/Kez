#include <ui/argparser/argparser.hpp>

static argparse::ArgumentParser selfcheck_parser("selfcheck");
argparse::ArgumentParser& get_selfcheck_parser() {
    selfcheck_parser.add_description("Run self-checks on the Fromager installation");
    return selfcheck_parser;
}

void execute_selfcheck_parser() {
    EXE_AND_CHECK("${FROMAGER_HOME}/bin/fromager_db_check");
    exit(EXIT_SUCCESS);
}

std::vector<std::string> get_selfcheck_suggestions(const int comp_cword,
                                                   const std::vector<std::string>& comp_words) {
    if (comp_cword == 2) {
        std::vector<std::string> suggestions = {"-h", "--help"};
        return suggestions;
    }
    return {};
}