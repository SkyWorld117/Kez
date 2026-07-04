#include <database/build_parser.hpp>
#include <database/condition_parser.hpp>
#include <database/config_parser.hpp>
#include <database/parser_utils.hpp>
#include <database/source_parser.hpp>
#include <memory>
#include <unordered_set>
#include <utility>

static PackageType parse_package_type(const YAML::Node& node, const std::string& path,
                                      const DatabaseParserContext& context) {
    const std::string value = parse_scalar(node, path, context);
    if (value == "package") {
        return PackageType::Package;
    }
    if (value == "system") {
        return PackageType::System;
    }
    if (value == "compiler") {
        return PackageType::Compiler;
    }
    if (value == "mpi") {
        return PackageType::Mpi;
    }
    if (value == "vendor") {
        return PackageType::Vendor;
    }
    if (value == "abstract") {
        return PackageType::Abstract;
    }
    if (value == "external") {
        return PackageType::External;
    }
    fail_config(node, path, "has unsupported package type '" + value + "'", context);
}

static std::unique_ptr<PackageConfig> make_package_config(const YAML::Node& recipe,
                                                          const DatabaseParserContext& context) {
    if (!has_key(recipe, "toolchain")) {
        return std::make_unique<GenericPackageConfig>();
    }

    const YAML::Node node   = recipe["toolchain"];
    const std::string value = parse_scalar(node, "recipe.toolchain", context);
    if (value == "autotools") {
        return std::make_unique<AutotoolsPackageConfig>();
    }
    if (value == "cmake") {
        return std::make_unique<CMakePackageConfig>();
    }
    if (value == "makefile") {
        return std::make_unique<MakefilePackageConfig>();
    }
    if (value == "make") {
        return std::make_unique<MakePackageConfig>();
    }
    fail_config(node, "recipe.toolchain", "has unsupported toolchain '" + value + "'", context);
}

static std::vector<Override> parse_overrides(const YAML::Node& node, const std::string& path,
                                             const DatabaseParserContext& context) {
    expect_sequence(node, path, context);
    std::vector<Override> result;
    for (std::size_t i = 0; i < node.size(); ++i) {
        const std::string override_path = path + "[" + std::to_string(i) + "]";
        YAML::Node override_node        = node[i];
        expect_map(override_node, override_path, context);
        check_keys(override_node, {"condition", "target", "action", "value"}, override_path,
                   context);

        Override value;
        value.condition = optional_scalar(override_node, "condition", override_path, context);
        if (value.condition.has_value()) {
            validate_condition(*value.condition, override_node["condition"],
                               override_path + ".condition", context);
        }
        value.target = required_scalar(override_node, "target", override_path, context);
        if (has_key(override_node, "action")) {
            value.action =
                parse_action(override_node["action"], override_path + ".action", context);
        }
        value.value = required_scalar(override_node, "value", override_path, context);
        result.push_back(std::move(value));
    }
    return result;
}

static std::vector<Property> parse_properties(const YAML::Node& node, const std::string& path,
                                              const DatabaseParserContext& context) {
    if (node.IsNull()) {
        return {};
    }
    expect_map(node, path, context);
    std::unordered_set<std::string> names;
    std::vector<Property> result;
    for (const auto& entry : node) {
        if (!entry.first.IsScalar()) {
            fail_config(entry.first, path, "contains a non-scalar property name", context);
        }
        Property property;
        property.name = entry.first.Scalar();
        if (!names.emplace(property.name).second) {
            fail_config(entry.first, path + "." + property.name, "is duplicated", context);
        }
        const std::string property_path = path + "." + property.name;
        if (entry.second.IsScalar()) {
            property.data = entry.second.Scalar();
        } else if (entry.second.IsMap()) {
            property.data = parse_string_configurable(entry.second, property_path, context);
        } else {
            fail_config(entry.second, property_path, "must be a scalar or configurable value map",
                        context);
        }
        result.push_back(std::move(property));
    }
    return result;
}

PackageConfigPtr parse_config_document(const YAML::Node& document,
                                       const DatabaseParserContext& context) {
    expect_map(document, "document", context);
    check_keys(document, {"recipe"}, "document", context);
    YAML::Node recipe = required_node(document, "recipe", "document", context);
    expect_map(recipe, "recipe", context);
    check_keys(recipe,
               {"name", "description", "author", "type", "toolchain", "source", "dependencies",
                "overrides", "build", "properties", "implementations"},
               "recipe", context);

    validate_templates(document, "document", context);

    std::unique_ptr<PackageConfig> config = make_package_config(recipe, context);
    config->name                          = required_scalar(recipe, "name", "recipe", context);
    if (config->name.empty()) {
        fail_config(recipe["name"], "recipe.name", "must not be empty", context);
    }
    config->description = optional_scalar(recipe, "description", "recipe", context);
    config->author      = optional_scalar(recipe, "author", "recipe", context);
    config->type        = parse_package_type(required_node(recipe, "type", "recipe", context),
                                             "recipe.type", context);

    if (has_key(recipe, "source")) {
        config->source = parse_source(recipe["source"], "recipe.source", context);
    }
    if (has_key(recipe, "dependencies")) {
        config->dependencies =
            parse_scalar_sequence(recipe["dependencies"], "recipe.dependencies", context);
    }
    if (has_key(recipe, "overrides")) {
        config->overrides = parse_overrides(recipe["overrides"], "recipe.overrides", context);
    }
    if (has_key(recipe, "build")) {
        config->build = parse_build(recipe["build"], "recipe.build", context);
    }
    if (has_key(recipe, "properties")) {
        config->properties = parse_properties(recipe["properties"], "recipe.properties", context);
    }
    if (has_key(recipe, "implementations")) {
        config->implementations =
            parse_scalar_sequence(recipe["implementations"], "recipe.implementations", context);
    }

    if (config->type == PackageType::Abstract && config->implementations.empty()) {
        fail_config(recipe, "recipe", "abstract packages require at least one implementation",
                    context);
    }

    return PackageConfigPtr(std::move(config));
}

PackageConfigPtr parse_db_config(const std::filesystem::path& config_path) {
    YAML::Node document = YAML::LoadFile(config_path.string());
    return parse_config_document(document, DatabaseParserContext {config_path});
}
