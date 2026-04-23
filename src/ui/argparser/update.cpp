#include <ui/argparser/argparser.hpp>

static argparse::ArgumentParser update_parser("update");

argparse::ArgumentParser& get_update_parser() {
    update_parser.add_description("Update the Fromager environment by refreshing core utilities");
    update_parser.add_argument("--with-system")
        .help("Include system utilities in the update process (default: false)")
        .default_value(false)
        .implicit_value(true);
    return update_parser;
}

void execute_update_parser() {
    INFO("Updating Fromager...");
    EXE_AND_CHECK("cd ${FROMAGER_HOME} && git pull && make -B -j${FROMAGER_NPROC}");
    if (update_parser.get<bool>("--with-system")) {
        INFO("Updating system utilities...");
        EXE_AND_CHECK("${FROMAGER_HOME}/bin/fromager_init --refresh");
    }
    INFO("Fromager update completed successfully.");
    INFO("Reload the shell or run `source ${FROMAGER_HOME}/setup-env.sh` to apply the updates.");
    exit(EXIT_SUCCESS);
}

std::vector<std::string> get_update_suggestions(const int comp_cword,
                                                const std::vector<std::string>& comp_words) {
    if (comp_cword == 2) {
        std::vector<std::string> suggestions = {"--with-system", "-h", "--help"};
        return suggestions;
    }
    return {};
}