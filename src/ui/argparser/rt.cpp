#include <ui/argparser/argparser.hpp>

static argparse::ArgumentParser rt_parser("rt");
static argparse::ArgumentParser rt_factory_parser("factory");
static argparse::ArgumentParser rt_factory_create_parser("create");
static argparse::ArgumentParser rt_factory_remove_parser("remove");
static argparse::ArgumentParser rt_factory_list_parser("list");
static argparse::ArgumentParser rt_factory_enter_parser("enter");
static argparse::ArgumentParser rt_factory_exit_parser("exit");
static argparse::ArgumentParser rt_factory_which_parser("which");
static argparse::ArgumentParser rt_build_parser("build");
static argparse::ArgumentParser rt_taste_parser("taste");
static argparse::ArgumentParser rt_summarize_parser("summarize");
static argparse::ArgumentParser rt_try_parser("try");

argparse::ArgumentParser& get_rt_parser() {
    rt_parser.add_description("Manage rapid test factories");

    rt_factory_parser.add_description("Manage rapid test factories");

    rt_factory_create_parser.add_description("Create a new rapid test factory");
    rt_factory_create_parser.add_argument("factory_name");

    rt_factory_remove_parser.add_description("Remove an existing rapid test factory");
    rt_factory_remove_parser.add_argument("factory_name");

    rt_factory_list_parser.add_description("List all existing rapid test factories");

    rt_factory_enter_parser.add_description("Enter a rapid test factory to use its environment");
    rt_factory_enter_parser.add_argument("factory_name");

    rt_factory_exit_parser.add_description("Exit the currently entered rapid test factory");

    rt_factory_which_parser.add_description("Show the currently entered rapid test factory");

    rt_factory_parser.add_subparser(rt_factory_create_parser);
    rt_factory_parser.add_subparser(rt_factory_remove_parser);
    rt_factory_parser.add_subparser(rt_factory_list_parser);
    rt_factory_parser.add_subparser(rt_factory_enter_parser);
    rt_factory_parser.add_subparser(rt_factory_exit_parser);
    rt_factory_parser.add_subparser(rt_factory_which_parser);

    rt_try_parser.add_argument("config");
    rt_try_parser.add_argument("package");

    rt_parser.add_subparser(rt_factory_parser);
    rt_parser.add_subparser(rt_build_parser);
    rt_parser.add_subparser(rt_taste_parser);
    rt_parser.add_subparser(rt_summarize_parser);
    rt_parser.add_subparser(rt_try_parser);

    return rt_parser;
}

void execute_rt_parser() {
    if (rt_parser.is_subcommand_used("factory")) {
        if (rt_factory_parser.is_subcommand_used("create")) {
            std::string factory_name = rt_factory_create_parser.get<std::string>("factory_name");
            if (factory_name.empty()) {
                ERROR("Factory name cannot be empty.");
                exit(EXIT_FAILURE);
            }
            std::filesystem::path factory_path =
                global_config::get_path("factories") + "/" + factory_name;
            if (std::filesystem::exists(factory_path)) {
                ERROR("Factory already exists: " + factory_name);
                exit(EXIT_FAILURE);
            }
            std::filesystem::create_directories(factory_path);
            std::filesystem::create_directories(factory_path / "wheels");
            std::filesystem::create_directories(factory_path / "tasting_rooms");
            SUCCESS("Factory created: " + factory_name);
        }

        if (rt_factory_parser.is_subcommand_used("remove")) {
            std::string factory_name = rt_factory_remove_parser.get<std::string>("factory_name");
            if (factory_name.empty()) {
                ERROR("Factory name cannot be empty.");
                exit(EXIT_FAILURE);
            }
            std::filesystem::path factory_path =
                global_config::get_path("factories") + "/" + factory_name;
            if (!std::filesystem::exists(factory_path)) {
                ERROR("Factory does not exist: " + factory_name);
                exit(EXIT_FAILURE);
            }
            std::filesystem::remove_all(factory_path);
            SUCCESS("Factory removed: " + factory_name);
        }

        if (rt_factory_parser.is_subcommand_used("list")) {
            std::filesystem::path factories_path = global_config::get_path("factories");
            if (!std::filesystem::exists(factories_path)) {
                INFO("No factories found.");
                exit(EXIT_SUCCESS);
            }
            INFO("Available factories:");
            for (const auto& entry : std::filesystem::directory_iterator(factories_path)) {
                if (entry.is_directory()) {
                    std::cout << "  - " << entry.path().filename().string() << std::endl;
                }
            }
        }

        if (rt_factory_parser.is_subcommand_used("enter")) {
            std::string factory_name = rt_factory_enter_parser.get<std::string>("factory_name");
            if (factory_name.empty()) {
                ERROR("Factory name cannot be empty.");
                exit(EXIT_FAILURE);
            }
            std::filesystem::path factory_path =
                global_config::get_path("factories") + "/" + factory_name;
            if (!std::filesystem::exists(factory_path)) {
                ERROR("Factory does not exist: " + factory_name);
                exit(EXIT_FAILURE);
            }
            std::cout << "export FROMAGER_FACTORY=\"" << factory_name << "\"" << std::endl;
        }

        if (rt_factory_parser.is_subcommand_used("exit")) {
            std::string factory_name =
                std::getenv("FROMAGER_FACTORY") ? std::getenv("FROMAGER_FACTORY") : "";
            if (factory_name.empty()) {
                ERROR("No factory is currently entered.");
                exit(EXIT_FAILURE);
            }
            std::cout << "unset FROMAGER_FACTORY" << std::endl;
        }

        if (rt_factory_parser.is_subcommand_used("which")) {
            std::string factory_name =
                std::getenv("FROMAGER_FACTORY") ? std::getenv("FROMAGER_FACTORY") : "";
            if (factory_name.empty()) {
                INFO("Not currently in a factory.");
            } else {
                INFO("Currently in factory: " + factory_name);
            }
        }

        exit(EXIT_SUCCESS);
    }

    if (rt_parser.is_subcommand_used("build")) {
        INFO("Starting rapid test build process...");
        if (std::getenv("FROMAGER_FACTORY") == nullptr) {
            ERROR("No factory is currently entered. Please enter a factory to use its "
                  "environment for building.");
            exit(EXIT_FAILURE);
        }
        std::string factory_name = std::getenv("FROMAGER_FACTORY");
        std::filesystem::path factory_path =
            global_config::get_path("factories") + "/" + factory_name;

        if (!std::filesystem::exists(factory_path)) {
            ERROR("Factory does not exist: " + factory_name);
            exit(EXIT_FAILURE);
        }
        if (!std::filesystem::exists(factory_path / "wheels")) {
            ERROR("Factory is missing 'wheels' directory: " + factory_name);
            exit(EXIT_FAILURE);
        }
        std::filesystem::path wheels_path = factory_path / "wheels";
        if (std::filesystem::is_empty(wheels_path)) {
            ERROR("No wheels found in factory: " + factory_name);
            exit(EXIT_FAILURE);
        }

        for (const auto& entry : std::filesystem::directory_iterator(wheels_path)) {
            if (entry.is_regular_file() && entry.path().extension() == ".yaml") {
                std::string config_path = entry.path().string();
                INFO("Installing from configuration: " + config_path);
                YAML::Node config                   = YAML::LoadFile(config_path);
                std::filesystem::path target_cellar = factory_path / "cellar" / entry.path().stem();

                YAML::Node instructions_yaml = parse(config, "release", target_cellar.string());
                YAML::Emitter out;
                out << instructions_yaml;
                std::filesystem::path tmp_path = target_cellar / ".tmp";
                std::filesystem::create_directories(tmp_path);
                std::ofstream ofs((tmp_path / "ins.yaml").string());
                if (!ofs) {
                    ERROR("Failed to create instruction file for config: " + config_path);
                    exit(EXIT_FAILURE);
                }
                ofs << out.c_str();
                ofs.close();

                EXE_AND_CHECK("${FROMAGER_HOME}/bin/fromager_install " + target_cellar.string());

                SUCCESS("Installation from configuration " + config_path + " completed.");
            } else {
                WARNING("Skipping non-configuration file in wheels directory: " +
                        entry.path().string());
            }
        }

        SUCCESS("Rapid test build process completed.");
        exit(EXIT_SUCCESS);
    }

    if (rt_parser.is_subcommand_used("taste")) {
        INFO("Tasting the cheese...");
        if (std::getenv("FROMAGER_FACTORY") == nullptr) {
            ERROR("No factory is currently entered. Please enter a factory to use its "
                  "environment for tasting.");
            exit(EXIT_FAILURE);
        }
        std::string factory_name = std::getenv("FROMAGER_FACTORY");
        std::filesystem::path factory_path =
            global_config::get_path("factories") + "/" + factory_name;

        if (!std::filesystem::exists(factory_path)) {
            ERROR("Factory does not exist: " + factory_name);
            exit(EXIT_FAILURE);
        }

        if (!std::filesystem::exists(factory_path / "tasting_rooms")) {
            ERROR("Factory is missing 'tasting_rooms' directory: " + factory_name);
            exit(EXIT_FAILURE);
        }
        std::filesystem::path tasting_rooms_path = factory_path / "tasting_rooms";
        if (!std::filesystem::exists(tasting_rooms_path / "config.yaml")) {
            ERROR("Factory is missing tasting room configuration: " + factory_name);
            exit(EXIT_FAILURE);
        }

        EXE_AND_CHECK("${FROMAGER_HOME}/bin/fromager_rt_profile");
        SUCCESS("Tasting completed.");
    }

    if (rt_parser.is_subcommand_used("summarize")) {
        INFO("Summarizing the results...");
        if (std::getenv("FROMAGER_FACTORY") == nullptr) {
            ERROR("No factory is currently entered. Please enter a factory to use its "
                  "environment for summarizing.");
            exit(EXIT_FAILURE);
        }
        std::string factory_name = std::getenv("FROMAGER_FACTORY");
        std::filesystem::path factory_path =
            global_config::get_path("factories") + "/" + factory_name;

        if (!std::filesystem::exists(factory_path)) {
            ERROR("Factory does not exist: " + factory_name);
            exit(EXIT_FAILURE);
        }

        if (!std::filesystem::exists(factory_path / "tasting_rooms")) {
            ERROR("Factory is missing 'tasting_rooms' directory: " + factory_name);
            exit(EXIT_FAILURE);
        }
        std::filesystem::path tasting_rooms_path = factory_path / "tasting_rooms";

        EXE_AND_CHECK("${FROMAGER_HOME}/bin/fromager_rt_summarize");
        SUCCESS("Tasting summary completed.");
    }

    if (rt_parser.is_subcommand_used("try")) {
        if (std::getenv("FROMAGER_CELLAR") == nullptr) {
            ERROR("No cellar is currently entered. Please enter a cellar to use its "
                  "environment for trying the configuration.");
            exit(EXIT_FAILURE);
        }
        std::string cellar_name = std::getenv("FROMAGER_CELLAR");

        std::string config_path = rt_try_parser.get<std::string>("config");
        std::string package     = rt_try_parser.get<std::string>("package");

        if (!std::filesystem::exists(config_path)) {
            ERROR("Configuration file does not exist: " + config_path);
            exit(EXIT_FAILURE);
        }

        std::filesystem::path target_cellar =
            global_config::get_path("cellars") + "/" + cellar_name;
        std::filesystem::path tmp_path = target_cellar / ".tmp";
        std::filesystem::create_directories(tmp_path);
        YAML::Node config            = YAML::LoadFile(config_path);
        YAML::Node instructions_yaml = parse(config, "debug", tmp_path.string());

        YAML::Emitter out;
        out << instructions_yaml;
        std::ofstream ofs((tmp_path / "ins.yaml").string());
        if (!ofs) {
            ERROR("Failed to create instruction file for config: " + config_path);
            exit(EXIT_FAILURE);
        }
        ofs << out.c_str();
        ofs.close();

        EXE_AND_CHECK("${FROMAGER_HOME}/bin/fromager_rt_install " + target_cellar.string() + " " +
                      config_path + " " + package);

        SUCCESS("Rapid test try process completed.");
        exit(EXIT_SUCCESS);
    }
}