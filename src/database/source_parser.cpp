#include <database/parser_utils.hpp>
#include <database/source_parser.hpp>
#include <unordered_set>
#include <utility>

static SourceType parse_source_type(const YAML::Node& node, const std::string& path,
                                    const DatabaseParserContext& context) {
    const std::string value = parse_scalar(node, path, context);
    if (value == "git") {
        return SourceType::Git;
    }
    if (value == "tarball") {
        return SourceType::Tarball;
    }
    if (value == "zip") {
        return SourceType::Zip;
    }
    if (value == "script") {
        return SourceType::Script;
    }
    fail_config(node, path, "has unsupported source type '" + value + "'", context);
}

Source parse_source(const YAML::Node& node, const std::string& path,
                    const DatabaseParserContext& context) {
    expect_map(node, path, context);
    check_keys(node, {"type", "url", "releases"}, path, context);

    Source result;
    result.type =
        parse_source_type(required_node(node, "type", path, context), path + ".type", context);
    result.url = optional_scalar(node, "url", path, context);
    if (result.type == SourceType::Git && !result.url.has_value()) {
        fail_config(node, path + ".url", "is required for git sources", context);
    }

    YAML::Node releases = required_node(node, "releases", path, context);
    expect_sequence(releases, path + ".releases", context);
    std::unordered_set<std::string> versions;
    for (std::size_t i = 0; i < releases.size(); ++i) {
        const std::string release_path = path + ".releases[" + std::to_string(i) + "]";
        YAML::Node release_node        = releases[i];
        expect_map(release_node, release_path, context);
        check_keys(release_node, {"version", "url", "tag"}, release_path, context);

        Release release;
        release.version = required_scalar(release_node, "version", release_path, context);
        if (release.version.empty()) {
            fail_config(release_node["version"], release_path + ".version", "must not be empty",
                        context);
        }
        if (!versions.emplace(release.version).second) {
            fail_config(release_node["version"], release_path + ".version", "is duplicated",
                        context);
        }
        release.url = optional_scalar(release_node, "url", release_path, context);
        release.tag = optional_scalar(release_node, "tag", release_path, context);

        if (result.type == SourceType::Git && !release.tag.has_value()) {
            fail_config(release_node, release_path + ".tag", "is required for git releases",
                        context);
        }
        if ((result.type == SourceType::Tarball || result.type == SourceType::Zip) &&
            !release.url.has_value()) {
            fail_config(release_node, release_path + ".url",
                        "is required for tarball and zip releases", context);
        }
        result.releases.push_back(std::move(release));
    }
    if (result.releases.empty()) {
        fail_config(releases, path + ".releases", "must not be empty", context);
    }
    return result;
}

std::vector<std::string> parse_scalar_sequence(const YAML::Node& node, const std::string& path,
                                               const DatabaseParserContext& context) {
    expect_sequence(node, path, context);
    std::vector<std::string> result;
    result.reserve(node.size());
    for (std::size_t i = 0; i < node.size(); ++i) {
        result.push_back(parse_scalar(node[i], path + "[" + std::to_string(i) + "]", context));
    }
    return result;
}
