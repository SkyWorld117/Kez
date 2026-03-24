#include <ui/argparser/argparser.hpp>
#include <ui/bash_completion/utils.hpp>

static argparse::ArgumentParser cellar_parser("cellar");
static argparse::ArgumentParser cellar_create_parser("create");
static argparse::ArgumentParser cellar_remove_parser("remove");
static argparse::ArgumentParser cellar_list_parser("list");
static argparse::ArgumentParser cellar_enter_parser("enter");
static argparse::ArgumentParser cellar_exit_parser("exit");
static argparse::ArgumentParser cellar_which_parser("which");
static argparse::ArgumentParser cellar_empty_parser("empty");

argparse::ArgumentParser& get_cellar_parser() {
    cellar_parser.add_description("Manage per application environments");

    cellar_create_parser.add_description("Create a new cellar for an application");
    cellar_create_parser.add_argument("cellar_name");

    cellar_remove_parser.add_description("Remove an existing cellar");
    cellar_remove_parser.add_argument("cellar_name");

    cellar_list_parser.add_description("List all existing cellars");

    cellar_enter_parser.add_description("Enter a cellar to use its environment");
    cellar_enter_parser.add_argument("cellar_name");

    cellar_exit_parser.add_description("Exit the currently entered cellar");

    cellar_which_parser.add_description("Show the currently entered cellar");

    cellar_empty_parser.add_argument("cellar_name");

    cellar_parser.add_subparser(cellar_create_parser);
    cellar_parser.add_subparser(cellar_remove_parser);
    cellar_parser.add_subparser(cellar_list_parser);
    cellar_parser.add_subparser(cellar_enter_parser);
    cellar_parser.add_subparser(cellar_exit_parser);
    cellar_parser.add_subparser(cellar_which_parser);
    cellar_parser.add_subparser(cellar_empty_parser);

    return cellar_parser;
}

void execute_cellar_parser() {
    if (cellar_parser.is_subcommand_used("create")) {
        std::string cellar_name = cellar_create_parser.get<std::string>("cellar_name");
        if (cellar_name.empty()) {
            ERROR("Cellar name cannot be empty.");
            exit(EXIT_FAILURE);
        }
        if (cellar_name == "system" || cellar_name == "compilers" || cellar_name == "mpis" ||
            cellar_name == "vendors" || cellar_name == "utilities") {
            ERROR("Cannot create cellar with reserved name: " + cellar_name);
            exit(EXIT_FAILURE);
        }
        std::filesystem::path cellar_path = global_config::get_path("cellars") + "/" + cellar_name;
        if (std::filesystem::exists(cellar_path)) {
            ERROR("Cellar already exists: " + cellar_name);
            exit(EXIT_FAILURE);
        }
        std::filesystem::create_directories(cellar_path);
        SUCCESS("Cellar created: " + cellar_name);

        exit(EXIT_SUCCESS);
    }

    if (cellar_parser.is_subcommand_used("remove")) {
        std::string cellar_name = cellar_remove_parser.get<std::string>("cellar_name");
        if (cellar_name.empty()) {
            ERROR("Cellar name cannot be empty.");
            exit(EXIT_FAILURE);
        }
        std::filesystem::path cellar_path = global_config::get_path("cellars") + "/" + cellar_name;
        if (!std::filesystem::exists(cellar_path)) {
            ERROR("Cellar does not exist: " + cellar_name);
            exit(EXIT_FAILURE);
        }
        std::filesystem::remove_all(cellar_path);
        SUCCESS("Cellar removed: " + cellar_name);

        exit(EXIT_SUCCESS);
    }

    if (cellar_parser.is_subcommand_used("list")) {
        std::filesystem::path cellars_path = global_config::get_path("cellars");
        if (!std::filesystem::exists(cellars_path)) {
            INFO("No cellars found.");
            exit(EXIT_SUCCESS);
        }
        INFO("Available cellars:");
        for (const auto& entry : std::filesystem::directory_iterator(cellars_path)) {
            if (entry.is_directory() && entry.path().filename() != "system" &&
                entry.path().filename() != "compilers" && entry.path().filename() != "mpis" &&
                entry.path().filename() != "vendors" && entry.path().filename() != "utilities") {
                std::cout << "  - " << entry.path().filename().string() << std::endl;
            }
        }

        exit(EXIT_SUCCESS);
    }

    if (cellar_parser.is_subcommand_used("enter")) {
        std::string cellar_name = cellar_enter_parser.get<std::string>("cellar_name");
        if (cellar_name.empty()) {
            ERROR("Cellar name cannot be empty.");
            exit(EXIT_FAILURE);
        }
        if (cellar_name == "system" || cellar_name == "compilers" || cellar_name == "mpis" ||
            cellar_name == "vendors" || cellar_name == "utilities") {
            ERROR("Cannot enter reserved cellar: " + cellar_name);
            exit(EXIT_FAILURE);
        }
        std::filesystem::path cellar_path = global_config::get_path("cellars") + "/" + cellar_name;
        if (!std::filesystem::exists(cellar_path)) {
            ERROR("Cellar does not exist: " + cellar_name);
            exit(EXIT_FAILURE);
        }
        // Print out the command to set PATH for the entered cellar. The main shell script will evaluate this output and update the environment accordingly.
        std::cout << "export PATH=\"" << cellar_path.string()
                  << "/bin:$PATH\"; export FROMAGER_CELLAR=\"" << cellar_name << "\"" << std::endl;

        exit(EXIT_SUCCESS);
    }

    if (cellar_parser.is_subcommand_used("exit")) {
        // Print out the command to unset PATH for the exited cellar. The main shell script will evaluate this output and update the environment accordingly.
        std::string cellar_name =
            std::getenv("FROMAGER_CELLAR") ? std::getenv("FROMAGER_CELLAR") : "";
        if (cellar_name.empty()) {
            ERROR("No cellar is currently entered.");
            exit(EXIT_FAILURE);
        }
        if (cellar_name == "system" || cellar_name == "compilers" || cellar_name == "mpis" ||
            cellar_name == "vendors" || cellar_name == "utilities") {
            ERROR("Cannot exit reserved cellar: " + cellar_name);
            exit(EXIT_FAILURE);
        }
        std::filesystem::path cellar_path = global_config::get_path("cellars") + "/" + cellar_name;
        std::cout << "export PATH=\"$(echo $PATH | sed -e 's;" + cellar_path.string() +
                         "/bin:;;g')\"; unset FROMAGER_CELLAR"
                  << std::endl;

        exit(EXIT_SUCCESS);
    }

    if (cellar_parser.is_subcommand_used("which")) {
        std::string cellar_name =
            std::getenv("FROMAGER_CELLAR") ? std::getenv("FROMAGER_CELLAR") : "";
        if (cellar_name.empty()) {
            INFO("Not currently in a cellar.");
        } else {
            INFO("Currently in cellar: " + cellar_name);
        }

        exit(EXIT_SUCCESS);
    }

    if (cellar_parser.is_subcommand_used("empty")) {
        std::string cellar_name = cellar_empty_parser.get<std::string>("cellar_name");
        if (cellar_name.empty()) {
            ERROR("Cellar name cannot be empty.");
            exit(EXIT_FAILURE);
        }
        if (cellar_name == "system" || cellar_name == "compilers" || cellar_name == "mpis" ||
            cellar_name == "vendors" || cellar_name == "utilities") {
            ERROR("Cannot empty reserved cellar: " + cellar_name);
            exit(EXIT_FAILURE);
        }
        std::filesystem::path cellar_path = global_config::get_path("cellars") + "/" + cellar_name;
        if (!std::filesystem::exists(cellar_path)) {
            ERROR("Cellar does not exist: " + cellar_name);
            exit(EXIT_FAILURE);
        }
        for (const auto& entry : std::filesystem::directory_iterator(cellar_path)) {
            std::filesystem::remove_all(entry.path());
        }
        SUCCESS("Cellar has been emptied: " + cellar_name);

        exit(EXIT_SUCCESS);
    }
}

std::vector<std::string> get_cellar_suggestions(const int comp_cword,
                                                const std::vector<std::string>& comp_words) {
    if (comp_cword == 2) {
        std::vector<std::string> suggestions = {"create", "remove", "list", "enter", "exit",
                                                "which",  "empty",  "-h",   "--help"};
        return suggestions;
    } else if (comp_cword == 3 && (comp_words[2] == "create" || comp_words[2] == "remove" ||
                                   comp_words[2] == "enter" || comp_words[2] == "empty")) {
        std::vector<std::string> cellars = get_cellars_suggestions();
        return cellars;
    }
    return {};
}