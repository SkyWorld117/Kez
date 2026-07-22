#include <algorithm>
#include <database/config.hpp>
#include <filesystem>
#include <string>
#include <uconf_parser/parser_internal.hpp>
#include <utils/bash_utils.hpp>
#include <utils/yaml_utils.hpp>
#include <vector>

namespace {

    /**
     * @brief Find a release matching a given version string.
     *
     * Performs a linear search over the source's release list to locate the
     * entry whose @ref Release::version equals @p version.
     *
     * @param source  The source descriptor whose releases are searched.
     * @param version The exact version string to match (e.g. "3.1.4").
     *
     * @return A pointer to the matching @ref Release, or @c nullptr if no
     *         release with that version exists.
     */
    const Release* selected_release(const Source& source, const std::string& version) {
        const auto release = std::find_if(
            source.releases.begin(), source.releases.end(),
            [&version](const Release& candidate) { return candidate.version == version; });
        return release == source.releases.end() ? nullptr : &*release;
    }

    /**
     * @brief Extract the archive extension from a tarball URL.
     *
     * Scans the URL against a list of known archive extensions (``.tar.zst``,
     * ``.tar.bz2``, ``.tar.gz``, ``.tar.xz``, ``.tgz``, ``.tbz2``, ``.tbz``,
     * ``.tar``) and returns the first match.  Terminates with
     * ``user_config_error`` if no supported extension is found.
     */
    std::string tarball_extension(const std::string& url) {
        static const std::vector<std::string> extensions = {
            ".tar.zst", ".tar.bz2", ".tar.gz", ".tar.xz", ".tgz", ".tbz2", ".tbz", ".tar"};
        for (const std::string& extension : extensions) {
            if (url.find(extension) != std::string::npos) {
                return extension;
            }
        }
        user_config_error("tarball URL has no supported archive extension: " + url);
    }

    /**
     * @brief Append shell commands to fetch and unpack a remote source.
     *
     * Dispatches on the source type (``Git``, ``Script``, ``Zip``, or
     * tarball) to produce the appropriate download/extract commands.  Uses a
     * cached tarball of the previously-unpacked source tree when available to
     * avoid re-downloading.
     */
    void append_remote_source_commands(const ParsedUserPackage& package, const Release& release,
                                       UserConfigParserContext& context,
                                       std::vector<std::string>& commands) {
        // Attempt to use a cached tarball of the already-unpacked source tree.
        // Returns true and appends extract + cd commands when the cache exists.
        auto use_cache = [&](bool source_is_file) {
            std::string cache_name = package.requested_name + "-" + release.version;
            const std::filesystem::path cache_path =
                context.settings.cache_prefix / (cache_name + ".tar.gz");
            if (!std::filesystem::exists(cache_path)) {
                return false;
            }
            if (std::filesystem::is_directory(cache_path)) {
                user_config_error("cache path exists but is not a file: " + cache_path.string());
            }
            commands.push_back("tar -xzf " + shell_single_quote(cache_path.string()));
            if (!source_is_file) {
                commands.push_back("cd source");
            }
            return true;
        };

        // Archive the current `source` directory into the cache as a tarball.
        auto pack_as_cache = [&]() {
            std::string cache_name = package.requested_name + "-" + release.version;
            const std::filesystem::path cache_path =
                context.settings.cache_prefix / (cache_name + ".tar.gz");
            commands.push_back("mkdir -p " +
                               shell_single_quote(context.settings.cache_prefix.string()));
            commands.push_back("tar -czf " + shell_single_quote(cache_path.string()) +
                               " --format=posix -z source");
        };

        const Source& source = *package.database_config->source;
        if (source.type == SourceType::Git) {
            if (use_cache(false)) return;
            const std::string url = resolve_parser_scalar(*source.url, context);
            const std::string tag = resolve_parser_scalar(*release.tag, context);
            const std::filesystem::path helper =
                context.settings.kez_home / "tools" / "shallow_clone.sh";
            commands.push_back("bash " + shell_single_quote(helper.string()) + " " +
                               shell_single_quote(url) + " " + shell_single_quote(tag) + " source");
            pack_as_cache();
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
            if (use_cache(true)) return;
            commands.push_back(
                "wget --quiet --show-progress --no-check-certificate --output-document=source " +
                shell_single_quote(url));
            pack_as_cache();
            return;
        }

        const std::filesystem::path helper = context.settings.kez_home / "tools" / "unpack.sh";
        if (source.type == SourceType::Zip) {
            if (use_cache(false)) return;
            commands.push_back("wget --quiet --show-progress --no-check-certificate "
                               "--output-document=source.zip " +
                               shell_single_quote(url));
            commands.push_back("bash " + shell_single_quote(helper.string()) +
                               " source.zip source");
            commands.push_back("rm source.zip");
            pack_as_cache();
            commands.push_back("cd source");
            return;
        }

        if (use_cache(false)) return;
        const std::string extension = tarball_extension(url);
        const std::string archive   = "source" + extension;
        commands.push_back("wget --quiet --show-progress --no-check-certificate "
                           "--output-document=" +
                           shell_single_quote(archive) + " " + shell_single_quote(url));
        commands.push_back("bash " + shell_single_quote(helper.string()) + " " +
                           shell_single_quote(archive) + " source");
        commands.push_back("rm " + shell_single_quote(archive));
        pack_as_cache();
        commands.push_back("cd source");
    }

}  // namespace

/**
 * @brief Append all source-fetching commands for a parsed user package.
 *
 * Looks up the requested version in the database release list; if found,
 * delegates to ``append_remote_source_commands``.  Otherwise expects a
 * ``<version>@<local-path>`` form and copies the local directory as the
 * source tree.  Terminates via ``user_config_error`` when the version is
 * missing, unknown, or the local path does not exist.
 */
void append_source_commands(const ParsedUserPackage& package, UserConfigParserContext& context,
                            std::vector<std::string>& commands) {
    if (!package.database_config->source.has_value()) {
        return;
    }
    if (package.database_config->source->type == SourceType::PyPI) {
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
