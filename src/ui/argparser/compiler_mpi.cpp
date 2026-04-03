#include <ui/argparser/argparser.hpp>

static argparse::ArgumentParser compiler_parser("compiler");
static argparse::ArgumentParser compiler_load_parser("load");
static argparse::ArgumentParser compiler_unload_parser("unload");
static argparse::ArgumentParser compiler_list_parser("list");
static argparse::ArgumentParser compiler_which_parser("which");
static argparse::ArgumentParser compiler_remove_parser("remove");

argparse::ArgumentParser& get_compiler_parser() {
    compiler_parser.add_description("Manage compilers");

    compiler_load_parser.add_description("Load a compiler");
    compiler_load_parser.add_argument("compiler_name");

    compiler_unload_parser.add_description("Unload the currently loaded compiler");

    compiler_list_parser.add_description("List available compilers");

    compiler_which_parser.add_description("Show the currently loaded compiler");

    compiler_remove_parser.add_description("Remove a compiler from the system");
    compiler_remove_parser.add_argument("compiler_name");

    compiler_parser.add_subparser(compiler_load_parser);
    compiler_parser.add_subparser(compiler_unload_parser);
    compiler_parser.add_subparser(compiler_list_parser);
    compiler_parser.add_subparser(compiler_which_parser);
    compiler_parser.add_subparser(compiler_remove_parser);

    return compiler_parser;
}

void execute_compiler_parser() {
    if (compiler_parser.is_subcommand_used("load")) {
        std::string compiler_name = compiler_load_parser.get<std::string>("compiler_name");
        if (compiler_name.empty()) {
            ERROR("Compiler name cannot be empty.");
            exit(EXIT_FAILURE);
        }
        std::filesystem::path compiler_path =
            global_config::get_path("compilers") + "/" + compiler_name;
        if (!std::filesystem::exists(compiler_path)) {
            ERROR("Compiler does not exist: " + compiler_name);
            exit(EXIT_FAILURE);
        }
        // Print out the command to set PATH for the loaded compiler. The main shell script will evaluate this output and update the environment accordingly.
        std::cout << "export PATH=\"" << compiler_path.string()
                  << "/bin:$PATH\"; export FROMAGER_COMPILER=\"" << compiler_name << "\""
                  << std::endl;

        exit(EXIT_SUCCESS);
    }

    if (compiler_parser.is_subcommand_used("unload")) {
        // Print out the command to unset PATH for the unloaded compiler. The main shell script will evaluate this output and update the environment accordingly.
        std::string compiler_name =
            std::getenv("FROMAGER_COMPILER") ? std::getenv("FROMAGER_COMPILER") : "";
        if (compiler_name.empty()) {
            ERROR("No compiler is currently loaded.");
            exit(EXIT_FAILURE);
        }
        std::filesystem::path compiler_path =
            global_config::get_path("compilers") + "/" + compiler_name;
        std::cout << "export PATH=\"$(echo $PATH | sed -e 's;" + compiler_path.string() +
                         "/bin:;;g')\"; unset FROMAGER_COMPILER"
                  << std::endl;

        exit(EXIT_SUCCESS);
    }

    if (compiler_parser.is_subcommand_used("list")) {
        std::filesystem::path compilers_path = global_config::get_path("compilers");
        if (!std::filesystem::exists(compilers_path)) {
            INFO("No compilers found.");
            exit(EXIT_SUCCESS);
        }
        INFO("Available compilers:");
        for (const auto& entry : std::filesystem::directory_iterator(compilers_path)) {
            if (entry.is_directory()) {
                std::cout << "  - " << entry.path().filename().string() << std::endl;
            }
        }

        exit(EXIT_SUCCESS);
    }

    if (compiler_parser.is_subcommand_used("which")) {
        std::string compiler_name =
            std::getenv("FROMAGER_COMPILER") ? std::getenv("FROMAGER_COMPILER") : "";
        if (compiler_name.empty()) {
            INFO("No compiler is currently loaded.");
        } else {
            INFO("Currently loaded compiler: " + compiler_name);
        }

        exit(EXIT_SUCCESS);
    }

    if (compiler_parser.is_subcommand_used("remove")) {
        std::string compiler_name = compiler_remove_parser.get<std::string>("compiler_name");
        if (compiler_name.empty()) {
            ERROR("Compiler name cannot be empty.");
            exit(EXIT_FAILURE);
        }
        std::filesystem::path compiler_path =
            global_config::get_path("compilers") + "/" + compiler_name;
        if (!std::filesystem::exists(compiler_path)) {
            ERROR("Compiler does not exist: " + compiler_name);
            exit(EXIT_FAILURE);
        }
        std::filesystem::remove_all(compiler_path);
        SUCCESS("Compiler removed: " + compiler_name);

        exit(EXIT_SUCCESS);
    }
}

std::vector<std::string> get_compiler_suggestions(const int comp_cword,
                                                  const std::vector<std::string>& comp_words) {
    if (comp_cword == 2) {
        std::vector<std::string> suggestions = {"load",   "unload", "list",  "which",
                                                "remove", "-h",     "--help"};
        return suggestions;
    } else if (comp_cword == 3 && (comp_words[2] == "load" || comp_words[2] == "remove")) {
        std::vector<std::string> suggestions = {"--help", "-h"};

        std::filesystem::path compilers_path = global_config::get_path("compilers");
        if (std::filesystem::exists(compilers_path) &&
            std::filesystem::is_directory(compilers_path)) {
            for (const auto& entry : std::filesystem::directory_iterator(compilers_path)) {
                if (entry.is_directory()) {
                    suggestions.push_back(entry.path().filename().string());
                }
            }
        }

        return suggestions;
    }
    return {};
}

static argparse::ArgumentParser mpi_parser("mpi");
static argparse::ArgumentParser mpi_load_parser("load");
static argparse::ArgumentParser mpi_unload_parser("unload");
static argparse::ArgumentParser mpi_list_parser("list");
static argparse::ArgumentParser mpi_which_parser("which");
static argparse::ArgumentParser mpi_remove_parser("remove");

argparse::ArgumentParser& get_mpi_parser() {
    mpi_parser.add_description("Manage MPI implementations");

    mpi_load_parser.add_description("Load an MPI implementation");
    mpi_load_parser.add_argument("mpi_name");

    mpi_unload_parser.add_description("Unload the currently loaded MPI implementation");

    mpi_list_parser.add_description("List available MPI implementations");

    mpi_which_parser.add_description("Show the currently loaded MPI implementation");

    mpi_remove_parser.add_description("Remove an MPI implementation from the system");
    mpi_remove_parser.add_argument("mpi_name");

    mpi_parser.add_subparser(mpi_load_parser);
    mpi_parser.add_subparser(mpi_unload_parser);
    mpi_parser.add_subparser(mpi_list_parser);
    mpi_parser.add_subparser(mpi_which_parser);
    mpi_parser.add_subparser(mpi_remove_parser);

    return mpi_parser;
}

void execute_mpi_parser() {
    // Similar handling for MPI implementations can be implemented here following the pattern of compilers
    if (mpi_parser.is_subcommand_used("load")) {
        std::string mpi_name = mpi_load_parser.get<std::string>("mpi_name");
        if (mpi_name.empty()) {
            ERROR("MPI implementation name cannot be empty.");
            exit(EXIT_FAILURE);
        }
        std::filesystem::path mpi_path = global_config::get_path("mpis") + "/" + mpi_name;
        if (!std::filesystem::exists(mpi_path)) {
            ERROR("MPI implementation does not exist: " + mpi_name);
            exit(EXIT_FAILURE);
        }
        // Print out the command to set PATH for the loaded MPI implementation. The main shell script will evaluate this output and update the environment accordingly.
        std::cout << "export PATH=\"" << mpi_path.string() << "/bin:$PATH\"; export FROMAGER_MPI=\""
                  << mpi_name << "\"" << std::endl;

        exit(EXIT_SUCCESS);
    }

    if (mpi_parser.is_subcommand_used("unload")) {
        // Print out the command to unset PATH for the unloaded MPI implementation. The main shell script will evaluate this output and update the environment accordingly.
        std::string mpi_name = std::getenv("FROMAGER_MPI") ? std::getenv("FROMAGER_MPI") : "";
        if (mpi_name.empty()) {
            ERROR("No MPI implementation is currently loaded.");
            exit(EXIT_FAILURE);
        }
        std::filesystem::path mpi_path = global_config::get_path("mpis") + "/" + mpi_name;
        std::cout << "export PATH=\"$(echo $PATH | sed -e 's;" + mpi_path.string() +
                         "/bin:;;g')\"; unset FROMAGER_MPI"
                  << std::endl;

        exit(EXIT_SUCCESS);
    }

    if (mpi_parser.is_subcommand_used("list")) {
        std::filesystem::path mpis_path = global_config::get_path("mpis");
        if (!std::filesystem::exists(mpis_path)) {
            INFO("No MPI implementations found.");
            exit(EXIT_SUCCESS);
        }
        INFO("Available MPI implementations:");
        for (const auto& entry : std::filesystem::directory_iterator(mpis_path)) {
            if (entry.is_directory()) {
                std::cout << "  - " << entry.path().filename().string() << std::endl;
            }
        }

        exit(EXIT_SUCCESS);
    }

    if (mpi_parser.is_subcommand_used("which")) {
        std::string mpi_name = std::getenv("FROMAGER_MPI") ? std::getenv("FROMAGER_MPI") : "";
        if (mpi_name.empty()) {
            INFO("No MPI implementation is currently loaded.");
        } else {
            INFO("Currently loaded MPI implementation: " + mpi_name);
        }

        exit(EXIT_SUCCESS);
    }

    if (mpi_parser.is_subcommand_used("remove")) {
        std::string mpi_name = mpi_remove_parser.get<std::string>("mpi_name");
        if (mpi_name.empty()) {
            ERROR("MPI implementation name cannot be empty.");
            exit(EXIT_FAILURE);
        }
        std::filesystem::path mpi_path = global_config::get_path("mpis") + "/" + mpi_name;
        if (!std::filesystem::exists(mpi_path)) {
            ERROR("MPI implementation does not exist: " + mpi_name);
            exit(EXIT_FAILURE);
        }
        std::filesystem::remove_all(mpi_path);
        SUCCESS("MPI implementation removed: " + mpi_name);

        exit(EXIT_SUCCESS);
    }
}

std::vector<std::string> get_mpi_suggestions(const int comp_cword,
                                             const std::vector<std::string>& comp_words) {
    if (comp_cword == 2) {
        std::vector<std::string> suggestions = {"load",   "unload", "list",  "which",
                                                "remove", "-h",     "--help"};
        return suggestions;
    } else if (comp_cword == 3 && (comp_words[2] == "load" || comp_words[2] == "remove")) {
        std::vector<std::string> suggestions = {"--help", "-h"};

        std::filesystem::path mpis_path = global_config::get_path("mpis");
        if (std::filesystem::exists(mpis_path) && std::filesystem::is_directory(mpis_path)) {
            for (const auto& entry : std::filesystem::directory_iterator(mpis_path)) {
                if (entry.is_directory()) {
                    suggestions.push_back(entry.path().filename().string());
                }
            }
        }

        return suggestions;
    }
    return {};
}