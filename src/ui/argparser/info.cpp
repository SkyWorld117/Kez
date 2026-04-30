#include <global_config.hpp>
#include <ui/argparser/argparser.hpp>
#include <ui/bash_completion/utils.hpp>

static argparse::ArgumentParser info_parser("info");

void print_report() {
    int max_width   = 80;
    int short_width = 30;
    int indent      = 4;
    int tab         = 30;

    auto strong_color = [](const std::string& txt) {
        return color<Color::BOLD, Color::MAGENTA>(txt);
    };
    auto normal_color = [](const std::string& txt) { return color<Color::MAGENTA>(txt); };

    std::string package_name = info_parser.get<std::string>("package");

    YAML::Node package = get_db_config(package_name);

    std::string large_delimiter = "";
    std::string short_delimiter = "";
    for (int i = 0; i < max_width; ++i) {
        large_delimiter += "=";
    }
    for (int i = 0; i < short_width; ++i) {
        short_delimiter += "-";
    }

    print_text(strong_color(large_delimiter));
    print_text(strong_color(package["cheese"]["name"].as<std::string>()), max_width);
    print_text(strong_color(large_delimiter));
    print_text(package["cheese"]["description"].as<std::string>(), max_width);
    print_text("");
    print_text(normal_color("Author: ") + "{missing}", max_width);
    print_text("");
    if (package["cheese"]["type"]) {
        print_text(normal_color("Type: ") + package["cheese"]["type"].as<std::string>(), max_width);
    } else {
        print_text(normal_color("Type: ") + "{unkown}", max_width);
    }
    if (package["cheese"]["toolchain"]) {
        print_text(normal_color("Toolchain: ") + package["cheese"]["toolchain"].as<std::string>(),
                   max_width);
    } else {
        print_text(normal_color("Toolchain: ") + "{unknown}", max_width);
    }
    print_text("");
    print_text(strong_color(short_delimiter));
    print_text(strong_color("Releases:"));
    print_text("");

    std::vector<YAML::Node> versions =
        package["cheese"]["source"]["releases"].as<std::vector<YAML::Node>>();
    for (int i = 0; i < versions.size(); ++i) {
        print_text("Version " + normal_color(versions[i]["version"].as<std::string>()), max_width);
        print_text(versions[i]["url"].as<std::string>(), 0, indent);
    }

    print_text("");
    print_text(strong_color(short_delimiter));
    print_text(strong_color("Config Options:"));
    print_text("");

    std::vector<YAML::Node> options;
    if (package["cheese"]["build"]["configurations"]["options"]) {
        // This approach here will create copies of the elements, so it might not
        // be the most efficient one, but it simplifies the code quite some.
        std::vector<YAML::Node> config_options =
            package["cheese"]["build"]["configurations"]["options"].as<std::vector<YAML::Node>>();
        options.insert(options.end(), config_options.begin(), config_options.end());
    }

    if (package["cheese"]["build"]["stages"]) {
        std::vector<YAML::Node> targets =
            package["cheese"]["build"]["stages"].as<std::vector<YAML::Node>>();
        for (int i = 0; i < targets.size(); ++i) {
            if (targets[i]["configurations"]) {
                std::vector<YAML::Node> config_target =
                    targets[i]["configurations"]["options"].as<std::vector<YAML::Node>>();
                options.insert(options.end(), config_target.begin(), config_target.end());
            }
        }
    }

    for (int i = 0; i < options.size(); ++i) {
        // check if it even is user configurable
        if (!options[i]["user_configurable"] || options[i]["user_configurable"].as<bool>()) {
            print_two_columns(options[i]["name"].as<std::string>(),
                              options[i]["description"].as<std::string>(), short_width, max_width);
        }
    }

    print_text("");
    print_text(strong_color(short_delimiter));
    print_text(strong_color("Dependencies:"));
    print_text("");

    if (package["cheese"]["dependencies"]) {
        std::vector<YAML::Node> dependencies =
            package["cheese"]["dependencies"].as<std::vector<YAML::Node>>();
        for (int i = 0; i < dependencies.size(); ++i) {
            print_two_columns(dependencies[i].as<std::string>(), "{Short description}", short_width,
                              max_width);
        }
    } else {
        print_text("None");
    }

    print_text("");
    print_text(strong_color(short_delimiter));
    print_text(strong_color("Properties:"));
    print_text("");

    for (YAML::const_iterator it = package["cheese"]["properties"].begin();
         it != package["cheese"]["properties"].end(); ++it) {
        if (package["cheese"]["properties"][it->first.as<std::string>()].Type() ==
            YAML::NodeType::Map) {
            print_two_columns(
                it->first.as<std::string>(),
                package["cheese"]["properties"][it->first.as<std::string>()]["default"]
                    .as<std::string>(),
                short_width, 0);
        } else {
            print_two_columns(it->first.as<std::string>(), it->second.as<std::string>(),
                              short_width, 0);
        }
    }

    print_text("");
}

void print_raw() {
    std::string package_name = info_parser.get<std::string>("package");
    YAML::Node package       = get_db_config(package_name);

    std::cout << package << "\n";
    print_text("");
}

argparse::ArgumentParser& get_info_parser() {
    info_parser.add_description("Get information about a package");
    info_parser.add_argument("package").help("Package name to get info for");
    info_parser.add_argument("-r", "--raw")
        .help("Print raw yaml instead of report")
        .default_value(false)
        .implicit_value(true);
    return info_parser;
}

void execute_info_parser() {
    if (info_parser.get<bool>("--raw")) {
        print_raw();
    } else {
        print_report();
    }
}

std::vector<std::string> get_info_suggestions(const int comp_cword,
                                              const std::vector<std::string>& comp_words) {
    // We always return the database packages as suggestions.
    return get_database_suggestions();
}