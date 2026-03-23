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