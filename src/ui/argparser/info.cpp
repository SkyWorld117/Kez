#include <global_config.hpp>
#include <ui/argparser/argparser.hpp>
#include <ui/bash_completion/utils.hpp>

static argparse::ArgumentParser info_parser("info");

static const int max_width   = 80;
static const int short_width = 30;
static const int indent      = 4;
static const int tab         = 30;

std::string strong_color(const std::string& txt) { return color<Color::BOLD, Color::MAGENTA>(txt); }

std::string normal_color(const std::string& txt) { return color<Color::MAGENTA>(txt); }

void print_title(const YAML::Node& package) {
    std::string large_delimiter(max_width, '=');

    print_text(strong_color(large_delimiter));
    print_text(strong_color(package["cheese"]["name"].as<std::string>()), max_width);
    print_text(strong_color(large_delimiter));

    if (package["cheese"]["description"]) {
        print_text(package["cheese"]["description"].as<std::string>(), max_width);
    }
    print_text("");
    print_text(normal_color("Author: ") + "{missing}", max_width);
    print_text("");

    std::string pkg_type = "{unkown}";
    if (package["cheese"]["type"]) {
        pkg_type = package["cheese"]["type"].as<std::string>();
    }
    print_text(normal_color("Type: ") + pkg_type, max_width);

    if (package["cheese"]["toolchain"]) {
        print_text(normal_color("Toolchain: ") + package["cheese"]["toolchain"].as<std::string>(),
                   max_width);
    }
}

void print_implements(const YAML::Node& package) {
    std::string short_delimiter(short_width, '-');

    print_text("");
    print_text(strong_color(short_delimiter));
    print_text(strong_color("Implements:"));
    print_text("");

    if (package["cheese"]["implementations"]) {
        std::vector<YAML::Node> implements =
            package["cheese"]["implementations"].as<std::vector<YAML::Node>>();

        if (implements.size()) {
            for (int i = 0; i < implements.size(); ++i) {
                print_two_columns(implements[i].as<std::string>(), "{Short description}",
                                  short_width, max_width);
            }
        } else {
            print_text("Nothing");
        }

    } else {
        print_text("Nothing");
    }
}

void print_properties(const YAML::Node& package) {
    std::string short_delimiter(short_width, '-');

    print_text("");
    print_text(strong_color(short_delimiter));
    print_text(strong_color("Properties:"));
    print_text("");

    if (package["cheese"]["properties"]) {
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
    } else {
        print_text("None");
    }
}

void print_releases(const YAML::Node& package) {
    std::string short_delimiter(short_width, '-');

    print_text("");
    print_text(strong_color(short_delimiter));
    print_text(strong_color("Releases:"));
    print_text("");

    if (package["cheese"]["source"]) {
        std::string release_type = package["cheese"]["source"]["type"].as<std::string>();
        print_text("Type: " + normal_color(release_type), max_width);
        if (release_type == "git") {
            print_text("Repository: " +
                       normal_color(package["cheese"]["source"]["url"].as<std::string>()));
            std::vector<YAML::Node> versions =
                package["cheese"]["source"]["releases"].as<std::vector<YAML::Node>>();
            for (int i = 0; i < versions.size(); ++i) {
                print_text("Version " + normal_color(versions[i]["version"].as<std::string>()),
                           max_width);
                print_text(versions[i]["tag"].as<std::string>(), 0, indent);
            }
        } else {
            std::vector<YAML::Node> versions =
                package["cheese"]["source"]["releases"].as<std::vector<YAML::Node>>();
            for (int i = 0; i < versions.size(); ++i) {
                print_text("Version " + normal_color(versions[i]["version"].as<std::string>()),
                           max_width);
                print_text(versions[i]["url"].as<std::string>(), 0, indent);
            }
        }
    } else {
        print_text("None");
    }
}

void print_options(const YAML::Node& package) {
    std::string short_delimiter(short_width, '-');

    print_text("");
    print_text(strong_color(short_delimiter));
    print_text(strong_color("Config Options:"));
    print_text("");

    std::vector<YAML::Node> options;
    if (package["cheese"] && package["cheese"]["build"] &&
        package["cheese"]["build"]["configurations"]) {
        // This approach here will create copies of the elements, so it might not
        // be the most efficient one, but it simplifies the code quite some.
        std::vector<YAML::Node> config_options =
            package["cheese"]["build"]["configurations"]["options"].as<std::vector<YAML::Node>>();
        options.insert(options.end(), config_options.begin(), config_options.end());
    }

    if (package["cheese"] && package["cheese"]["build"] && package["cheese"]["build"]["stages"]) {
        std::vector<YAML::Node> targets =
            package["cheese"]["build"]["stages"].as<std::vector<YAML::Node>>();
        for (int i = 0; i < targets.size(); ++i) {
            if (targets[i]["configurations"]) {
                if (targets[i]["configurations"]["options"]) {
                    std::vector<YAML::Node> config_target =
                        targets[i]["configurations"]["options"].as<std::vector<YAML::Node>>();
                    options.insert(options.end(), config_target.begin(), config_target.end());
                } else if (targets[i]["configurations"]["environments"]) {
                    std::vector<YAML::Node> config_target =
                        targets[i]["configurations"]["environments"].as<std::vector<YAML::Node>>();
                    options.insert(options.end(), config_target.begin(), config_target.end());
                }
            }
        }
    }

    if (options.size()) {
        for (int i = 0; i < options.size(); ++i) {
            // check if it even is user configurable
            if (!options[i]["user_configurable"] || options[i]["user_configurable"].as<bool>()) {
                std::string name = normal_color(options[i]["name"].as<std::string>());
                if (options[i]["enabled_value"] && options[i]["enabled_value"]["default"]) {
                    name += " [" + options[i]["enabled_value"]["default"].as<std::string>() + "]";
                }
                std::string description = "";
                if (options[i]["description"]) {
                    description = options[i]["description"].as<std::string>();
                }
                print_two_columns(name, description, short_width, max_width);
            }
        }
    } else {
        print_text("None");
    }
}

void print_dependencies(const YAML::Node& package) {
    std::string short_delimiter(short_width, '-');

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
}

void print_report() {
    std::string package_name = info_parser.get<std::string>("package");

    YAML::Node package = get_db_config(package_name);

    print_title(package);
    std::string pkg_type = "";
    if (package["cheese"]["type"]) {
        pkg_type = package["cheese"]["type"].as<std::string>();
    }

    if (pkg_type == "abstract") {
        print_implements(package);

    } else if (pkg_type == "external") {
        print_properties(package);
    } else {
        print_releases(package);
        print_options(package);
        print_dependencies(package);
        print_properties(package);
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