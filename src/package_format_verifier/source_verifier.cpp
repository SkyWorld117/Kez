#include <package_format_verifier/source_verifier.hpp>

bool verify_source(const YAML::Node& node) {
    if (!node.IsMap()) {
        ERROR("Root node must be a map.");
        return false;
    }

    if (!node["type"] || !node["type"].IsScalar()) {
        ERROR("Source must have a type as a scalar value.");
        return false;
    }

    if (node["type"].as<std::string>() != "tarball" && node["type"].as<std::string>() != "git" &&
        node["type"].as<std::string>() != "script") {
        ERROR("Unsupported source type: " + node["type"].as<std::string>());
        return false;
    }

    if (node["type"].as<std::string>() == "git" && !node["url"]) {
        ERROR("Source type 'git' must have a url.");
        return false;
    }

    if (!node["releases"] || !node["releases"].IsSequence()) {
        ERROR("Source must have releases as a sequence.");
        return false;
    }

    bool valid_releases = true;
    for (const auto& release : node["releases"]) {
        if (!verify_release(release, node["type"].as<std::string>())) {
            valid_releases = false;
        }
    }
    return valid_releases;
}

bool verify_release(const YAML::Node& node, const std::string& source_type) {
    if (!node.IsMap()) {
        ERROR("Release must be a map.");
        return false;
    }

    if (!node["version"] || !node["version"].IsScalar()) {
        ERROR("Release must have a version as a scalar value.");
        return false;
    }

    if (source_type == "git") {
        if (!node["tag"] || !node["tag"].IsScalar()) {
            ERROR("Git release must have a tag as a scalar value.");
            return false;
        }
    } else if (source_type == "tarball") {
        if (!node["url"] || !node["url"].IsScalar()) {
            ERROR("Tarball release must have a url as a scalar value.");
            return false;
        }
    }

    return true;
}