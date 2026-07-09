#include <database/parser_utils.hpp>
#include <database/source_parser.hpp>
#include <unordered_set>
#include <utility>

namespace {
    /**
     * @brief Converts a YAML scalar string to the corresponding SourceType
     *        enumerator.
     *
     * Accepts the four recognised source-type names ("git", "tarball", "zip",
     * "script") and maps each to its SourceType equivalent.  Any other value
     * is a fatal configuration error.
     *
     * @param node    The YAML scalar node whose value names the source type.
     * @param path    Logical YAML path used in error messages (e.g.
     *                "source.type").
     * @param context Parser context carrying the recipe tree root path for
     *                error annotation.
     *
     * @return The corresponding SourceType enumerator.
     *
     * @warning Terminates the program via fail_config if the value does not
     *          match any known source type.
     */
    SourceType parse_source_type(const YAML::Node& node, const std::string& path,
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
}  // namespace

/**
 * @brief Parses a single source block (type, URL, and releases) from a recipe's
 *        YAML configuration.
 *
 * Expects a map node containing at least "type" and "releases".  For git
 * sources the top-level "url" is required; for tarball/zip sources each
 * release entry must supply its own "url".  Duplicate version strings
 * across releases are rejected, and the releases sequence must not be empty.
 */
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
    // Track seen version strings to detect duplicates across releases.
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
        // Reject duplicate version strings within the same source block.
        if (!versions.emplace(release.version).second) {
            fail_config(release_node["version"], release_path + ".version", "is duplicated",
                        context);
        }
        release.url = optional_scalar(release_node, "url", release_path, context);
        release.tag = optional_scalar(release_node, "tag", release_path, context);

        // Type-specific per-release validation.
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

/**
 * @brief Parses a YAML sequence node into a vector of scalar strings.
 *
 * Validates that the node is a sequence, then extracts each element as a
 * scalar string.  Any element that is not a scalar triggers a fatal
 * configuration error via fail_config.
 */
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
