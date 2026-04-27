#include <filesystem>
#include <parser/source_parser.hpp>
#include <string>
#include <utils/colored_io.hpp>

void download_source(const YAML::Node pkg_config, const YAML::Node release,
                     const std::string package_name, const std::string source_type,
                     std::vector<std::string> &instructions) {
    if (source_type == "tarball") {
        // Case 1: Tarball
        // Tarball has `url` entry
        std::string url = release["url"].as<std::string>();
        if (url.find(".tar") == std::string::npos) {
            ERROR("Invalid tarball URL for package: " + package_name);
            exit(EXIT_FAILURE);
        }
        std::string ext = url.substr(url.find(".tar"));
        instructions.push_back("wget --quiet --show-progress --no-check-certificate "
                               "--output-document=source" +
                               ext + " " + url);
        instructions.push_back("mkdir -p source");
        instructions.push_back("bash $FROMAGER_HOME/patches/fgr_patch_source_structure.sh source" +
                               ext + " source");
        instructions.push_back("rm source" + ext);
        instructions.push_back("cd source");
    } else if (source_type == "git") {
        // Case 2: Git
        // Git has `tag` entry
        std::string git_url = pkg_config["cheese"]["source"]["url"].as<std::string>();
        std::string git_tag = release["tag"].as<std::string>();
        instructions.push_back("git clone " + git_url + " source");
        instructions.push_back("cd source");
        instructions.push_back("git checkout " + git_tag);
    } else if (source_type == "script" && release["url"]) {
        // Case 3: Script
        // Script may or may not have `url` entry
        // When not present, we assume the developer calls a script from `bin` at the preprocessing or postprocessing stage (no need to handle this case)
        // Else, we download the script
        std::string script_url = release["url"].as<std::string>();
        instructions.push_back("wget --quiet --show-progress --no-check-certificate "
                               "--output-document=source " +
                               script_url);
    } else if (source_type == "zip") {
        // Case 4: Zip
        // Zip has `url` entry
        std::string url = release["url"].as<std::string>();
        if (url.find(".zip") == std::string::npos) {
            ERROR("Invalid zip URL for package: " + package_name);
            exit(EXIT_FAILURE);
        }
        instructions.push_back("wget --quiet --show-progress --no-check-certificate "
                               "--output-document=source.zip " +
                               url);
        instructions.push_back("mkdir -p source");
        instructions.push_back("unzip -q source.zip -d source");
        instructions.push_back("rm source.zip");
        instructions.push_back("cd source");
    } else {
        // Handle unknown source types
        ERROR("Unknown source type for package: " + package_name);
        exit(EXIT_FAILURE);
    }
}

std::string get_source_path(std::string env_path) {
    std::filesystem::path env_path_fs(env_path);
    std::filesystem::path source_path = env_path_fs / ".tmp" / "source";
    return source_path.string();
}