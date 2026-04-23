#include <string>
#include <ui/argparser/argparser.hpp>
#include <ui/bash_completion/utils.hpp>

static argparse::ArgumentParser init_parser("init");

argparse::ArgumentParser& get_init_parser() {
    init_parser.add_description("Initialize the Fromager environment");
    init_parser.add_argument("--refresh")
        .help("Refresh the Fromager environment by reinstalling core utilities")
        .default_value(false)
        .implicit_value(true);
    init_parser.add_argument("--with-slurm")
        .help("Initialize the Fromager environment with Slurm support")
        .default_value(false)
        .implicit_value(true);

    return init_parser;
}

void execute_init_parser() {
    std::string cmd;

    if (init_parser.get<bool>("--refresh")) {
        INFO("Refreshing Fromager system environment...");
        WARNING("Note: This will delete the existing system environment and recreate it.");
        cmd = "${FROMAGER_HOME}/bin/fromager_init --refresh";
    } else {
        INFO("Initializing Fromager system environment...");
        cmd = "${FROMAGER_HOME}/bin/fromager_init";
    }

    if (init_parser.get<bool>("--with-slurm")) {
        cmd = "sbatch --ntasks=1 --cpus-per-task=${FROMAGER_NPROC} --time=01:00:00 --wrap=\"" +
              cmd + "\"";
    }

    EXE_AND_CHECK(cmd);

    exit(EXIT_SUCCESS);
}

std::vector<std::string> get_init_suggestions(const int comp_cword,
                                              const std::vector<std::string>& comp_words) {
    std::vector<std::string> suggestions;
    if (comp_cword == 2) {
        suggestions.push_back("--help");
    }
    if (comp_cword >= 2) {
        if (!exists_in(comp_words, "--refresh")) {
            suggestions.push_back("--refresh");
        }
        if (!exists_in(comp_words, "--with-slurm")) {
            suggestions.push_back("--with-slurm");
        }
    }
    return suggestions;
}