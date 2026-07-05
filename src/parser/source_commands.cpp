#include <algorithm>
#include <database/config.hpp>
#include <filesystem>
#include <string>
#include <vector>

#include "parser_internal.hpp"

namespace {

    const Release* selected_release(const Source& source, const std::string& version) {
        const auto release = std::find_if(
            source.releases.begin(), source.releases.end(),
            [&version](const Release& candidate) { return candidate.version == version; });
        return release == source.releases.end() ? nullptr : &*release;
    }

    std::string tarball_extension(const std::string& url) {
        static const std::vector<std::string> extensions = {".tar.zst", ".tar.bz2", ".tar.gz",
                                                            ".tar.xz",  ".tgz",     ".tar"};
        for (const std::string& extension : extensions) {
            if (url.find(extension) != std::string::npos) {
                return extension;
            }
        }
        user_config_error("tarball URL has no supported archive extension: " + url);
    }

    void append_remote_source_commands(const ParsedUserPackage& package, const Release& release,
                                       UserConfigParserContext& context,
                                       std::vector<std::string>& commands) {
        const Source& source = *package.database_config->source;
        if (source.type == SourceType::Git) {
            const std::string url = resolve_parser_scalar(*source.url, context);
            const std::string tag = resolve_parser_scalar(*release.tag, context);
            const std::filesystem::path helper =
                context.settings.kez_home / "tools" / "shallow_clone.sh";
            commands.push_back("bash " + shell_single_quote(helper.string()) + " " +
                               shell_single_quote(url) + " " + shell_single_quote(tag) + " source");
            commands.push_back("cd source");
            return;
        }

        if (!release.url.has_value()) {
            if (source.type == SourceType::Script) {
                return;
            }
            user_config_error("release '" + release.version + "' for package '" +
                              package.requested_name + "' has no URL");
        }

        const std::string url = resolve_parser_scalar(*release.url, context);
        if (source.type == SourceType::Script) {
            commands.push_back(
                "wget --quiet --show-progress --no-check-certificate --output-document=source " +
                shell_single_quote(url));
            return;
        }

        const std::filesystem::path helper = context.settings.kez_home / "tools" / "unpack.sh";
        if (source.type == SourceType::Zip) {
            commands.push_back("wget --quiet --show-progress --no-check-certificate "
                               "--output-document=source.zip " +
                               shell_single_quote(url));
            commands.push_back("bash " + shell_single_quote(helper.string()) +
                               " source.zip source");
            commands.push_back("rm source.zip");
            commands.push_back("cd source");
            return;
        }

        const std::string extension = tarball_extension(url);
        const std::string archive   = "source" + extension;
        commands.push_back("wget --quiet --show-progress --no-check-certificate "
                           "--output-document=" +
                           shell_single_quote(archive) + " " + shell_single_quote(url));
        commands.push_back("bash " + shell_single_quote(helper.string()) + " " +
                           shell_single_quote(archive) + " source");
        commands.push_back("rm " + shell_single_quote(archive));
        commands.push_back("cd source");
    }

}  // namespace

void append_source_commands(const ParsedUserPackage& package, UserConfigParserContext& context,
                            std::vector<std::string>& commands) {
    if (!package.database_config->source.has_value()) {
        return;
    }
    if (!yaml_has(package.user_config, "version")) {
        user_config_error("package '" + package.requested_name + "' is missing its version");
    }

    const std::string version = yaml_scalar(package.user_config["version"], "package version");
    const Release* release    = selected_release(*package.database_config->source, version);
    if (release != nullptr) {
        append_remote_source_commands(package, *release, context, commands);
        return;
    }

    const std::size_t separator = version.find('@');
    if (separator == std::string::npos || separator + 1 == version.size()) {
        user_config_error("package '" + package.requested_name + "' version '" + version +
                          "' is neither a known release nor '<version>@<local-path>'");
    }
    const std::filesystem::path source_path = version.substr(separator + 1);
    if (!std::filesystem::is_directory(source_path)) {
        user_config_error("local source directory does not exist: " + source_path.string());
    }
    commands.push_back("cp -a " + shell_single_quote(source_path.string()) + " source");
    commands.push_back("cd source");
}
