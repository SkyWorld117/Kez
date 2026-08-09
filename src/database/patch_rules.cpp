#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <database/parser_utils.hpp>
#include <database/patch_rules.hpp>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <utility>
#include <utils/colored_io.hpp>
#include <utils/string_utils.hpp>
#include <utils/yaml_utils.hpp>
#include <vector>

namespace {

    [[noreturn]] void patch_layout_error(const std::filesystem::path& path,
                                         const std::string& message) {
        ERROR(path.string() + ": " + message);
        exit(EXIT_FAILURE);
    }

    PatchVersionConstraint parse_version_constraint(const YAML::Node& node, const std::string& path,
                                                    const DatabaseParserContext& context) {
        const std::string value    = parse_scalar(node, path, context);
        std::size_t operation_size = 0;
        if (value.size() >= 2 && (value.substr(0, 2) == "==" || value.substr(0, 2) == ">=" ||
                                  value.substr(0, 2) == "<=")) {
            operation_size = 2;
        } else if (!value.empty() && (value.front() == '>' || value.front() == '<')) {
            operation_size = 1;
        }
        if (operation_size == 0) {
            fail_config(node, path, "must begin with ==, >=, >, <=, or <", context);
        }

        std::size_t version_start = operation_size;
        while (version_start < value.size() &&
               std::isspace(static_cast<unsigned char>(value[version_start]))) {
            ++version_start;
        }
        std::size_t version_end = value.size();
        while (version_end > version_start &&
               std::isspace(static_cast<unsigned char>(value[version_end - 1]))) {
            --version_end;
        }
        if (version_start == version_end) {
            fail_config(node, path, "is missing a version", context);
        }
        const std::string version = value.substr(version_start, version_end - version_start);
        if (version.front() == '=' || version.front() == '>' || version.front() == '<' ||
            std::any_of(version.begin(), version.end(),
                        [](unsigned char character) { return std::isspace(character); })) {
            fail_config(node, path, "contains an invalid version '" + version + "'", context);
        }
        return {value.substr(0, operation_size), version};
    }

    bool matches_constraint(const std::string& version, const PatchVersionConstraint& constraint) {
        const int comparison = compare_versions(version, constraint.version);
        if (constraint.operation == "==") return comparison == 0;
        if (constraint.operation == ">=") return comparison >= 0;
        if (constraint.operation == ">") return comparison > 0;
        if (constraint.operation == "<=") return comparison <= 0;
        if (constraint.operation == "<") return comparison < 0;
        return false;
    }

    bool applies_to_version(const PatchRule& rule, const std::string& version) {
        return std::all_of(rule.versions.begin(), rule.versions.end(),
                           [&version](const PatchVersionConstraint& constraint) {
                               return matches_constraint(version, constraint);
                           });
    }

}  // namespace

std::filesystem::path package_patch_directory(const PackageConfig& package) {
    return package.recipe_path.parent_path() / "patches";
}

std::vector<PatchRule> load_patch_rules(const std::filesystem::path& package_directory) {
    const std::filesystem::path patch_directory = package_directory / "patches";
    if (!std::filesystem::exists(patch_directory)) {
        return {};
    }
    if (!std::filesystem::is_directory(patch_directory)) {
        patch_layout_error(patch_directory, "must be a directory");
    }

    const std::filesystem::path rules_path = patch_directory / "_rules.yaml";
    if (!std::filesystem::is_regular_file(rules_path)) {
        patch_layout_error(rules_path, "is required when a package has a patches directory");
    }

    const DatabaseParserContext context {rules_path};
    const YAML::Node document = load_yaml_file(rules_path);
    check_keys(document, {"patches"}, "document", context);
    const YAML::Node patches = required_node(document, "patches", "document", context);
    expect_sequence(patches, "document.patches", context);

    std::vector<PatchRule> result;
    result.reserve(patches.size());
    std::unordered_set<std::string> names;
    names.reserve(patches.size());
    for (std::size_t index = 0; index < patches.size(); ++index) {
        const YAML::Node entry = patches[index];
        const std::string path = "document.patches[" + std::to_string(index) + "]";
        check_keys(entry, {"name", "enabled", "versions"}, path, context);

        PatchRule rule;
        rule.name = required_scalar(entry, "name", path, context);
        if (rule.name.empty() || rule.name == "." || rule.name == ".." ||
            rule.name == "_rules.yaml" ||
            std::filesystem::path(rule.name).filename() != rule.name) {
            fail_config(entry["name"], path + ".name", "must be a patch filename", context);
        }
        if (!names.emplace(rule.name).second) {
            fail_config(entry["name"], path + ".name", "duplicates patch '" + rule.name + "'",
                        context);
        }
        rule.enabled = parse_boolean(required_node(entry, "enabled", path, context),
                                     path + ".enabled", context);

        if (yaml_has(entry, "versions")) {
            const YAML::Node versions = entry["versions"];
            expect_sequence(versions, path + ".versions", context);
            for (std::size_t version_index = 0; version_index < versions.size(); ++version_index) {
                rule.versions.push_back(parse_version_constraint(
                    versions[version_index],
                    path + ".versions[" + std::to_string(version_index) + "]", context));
            }
        }

        const std::filesystem::path patch_path = patch_directory / rule.name;
        if (!std::filesystem::is_regular_file(patch_path)) {
            fail_config(entry["name"], path + ".name",
                        "references a missing patch file '" + patch_path.string() + "'", context);
        }
        result.push_back(std::move(rule));
    }

    for (const auto& entry : std::filesystem::directory_iterator(patch_directory)) {
        const std::string name = entry.path().filename().string();
        if (name == "_rules.yaml") {
            continue;
        }
        if (!entry.is_regular_file()) {
            patch_layout_error(entry.path(), "only regular patch files are allowed");
        }
        if (names.find(name) == names.end()) {
            patch_layout_error(entry.path(), "has no entry in _rules.yaml");
        }
    }

    std::sort(result.begin(), result.end(),
              [](const PatchRule& left, const PatchRule& right) { return left.name < right.name; });
    return result;
}

std::vector<PatchRule> applicable_patch_rules(const PackageConfig& package,
                                              const std::string& version) {
    std::vector<PatchRule> result = load_patch_rules(package.recipe_path.parent_path());
    result.erase(std::remove_if(result.begin(), result.end(),
                                [&version](const PatchRule& rule) {
                                    return !applies_to_version(rule, version);
                                }),
                 result.end());
    return result;
}
