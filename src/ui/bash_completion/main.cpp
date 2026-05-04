#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <ui/argparser/argparser.hpp>
#include <vector>

// This binary acts as an auto-completion engine.
// It receives the current COMP_WORDS from bash.
// Example: "fgr install --" or "fgr cell"

void suggest(const std::string& current_word, const std::vector<std::string>& words) {
    for (const auto& w : words) {
        if (w.rfind(current_word, 0) == 0) {
            std::cout << w << "\n";
        }
    }
}

int main(int argc, char** argv) {
    // argv[1] is the index of the word being completed (COMP_CWORD)
    // argv[2...] are the words (COMP_WORDS)
    if (argc < 3) return 0;

    int comp_cword = std::stoi(argv[1]);
    std::vector<std::string> comp_words;
    for (int i = 2; i < argc; ++i) {
        comp_words.push_back(argv[i]);
    }

    std::string current_word = comp_cword < comp_words.size() ? comp_words[comp_cword] : "";
    std::string cmd          = comp_words.size() > 1 ? comp_words[1] : "";
    std::string subcmd       = comp_words.size() > 2 ? comp_words[2] : "";
    std::string subsubcmd    = comp_words.size() > 3 ? comp_words[3] : "";

    std::set<std::string> main_commands = {
        "init", "selfcheck", "update", "utilities", "cellar", "install",   "template", "rt",
        "mpi",  "compiler",  "-h",     "--help",    "-v",     "--version", "info"};

    if (comp_cword == 1) {
        suggest(current_word, std::vector<std::string>(main_commands.begin(), main_commands.end()));
        return 0;
    }

    if (cmd == "init") {
        suggest(current_word, get_init_suggestions(comp_cword, comp_words));
    } else if (cmd == "selfcheck") {
        suggest(current_word, get_selfcheck_suggestions(comp_cword, comp_words));
    } else if (cmd == "update") {
        suggest(current_word, get_update_suggestions(comp_cword, comp_words));
    } else if (cmd == "utilities") {
        suggest(current_word, get_utilities_suggestions(comp_cword, comp_words));
    } else if (cmd == "cellar") {
        suggest(current_word, get_cellar_suggestions(comp_cword, comp_words));
    } else if (cmd == "compiler") {
        suggest(current_word, get_compiler_suggestions(comp_cword, comp_words));
    } else if (cmd == "mpi") {
        suggest(current_word, get_mpi_suggestions(comp_cword, comp_words));
    } else if (cmd == "install") {
        suggest(current_word, get_install_suggestions(comp_cword, comp_words));
    } else if (cmd == "template") {
        suggest(current_word, get_template_suggestions(comp_cword, comp_words));
    } else if (cmd == "rt") {
        suggest(current_word, get_rt_suggestions(comp_cword, comp_words));
    } else if (cmd == "info") {
        suggest(current_word, get_info_suggestions(comp_cword, comp_words));
    }

    return 0;
}
