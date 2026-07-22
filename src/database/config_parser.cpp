#include <algorithm>
#include <cctype>
#include <database/build_parser.hpp>
#include <database/condition_parser.hpp>
#include <database/config_parser.hpp>
#include <database/parser_utils.hpp>
#include <database/source_parser.hpp>
#include <memory>
#include <unordered_set>
#include <utility>
#include <utils/yaml_utils.hpp>

namespace {
    /**
     * @brief Parse a YAML scalar into a PackageType enumerator.
     *
     * Reads the string value of @p node and maps it to the corresponding
     * PackageType value.  The recognised strings are:
     *
     *   String       | Return value
     *   :----------- | :-----------
     *   `"package"`  | PackageType::Package
     *   `"system"`   | PackageType::System
     *   `"compiler"` | PackageType::Compiler
     *   `"mpi"`      | PackageType::Mpi
     *   `"vendor"`   | PackageType::Vendor
     *   `"abstract"` | PackageType::Abstract
     *   `"external"` | PackageType::External
     *
     * @param node    The YAML scalar node whose string value names the type.
     * @param path    Logical YAML path (e.g. `"recipe.type"`) used in error
     *                messages to identify the source location.
     * @param context Parser context carrying the recipe file path for
     *                diagnostic annotation.
     * @return The corresponding PackageType enumerator.
     *
     * @warning If the scalar value does not match any known type, the function
     *          calls @c fail_config() which prints an error and terminates the
     *          program with @c exit(EXIT_FAILURE).  It never returns normally
     *          on an unrecognised value.
     */
    PackageType parse_package_type(const YAML::Node& node, const std::string& path,
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

    std::unique_ptr<PackageConfig> make_package_config(const YAML::Node& recipe,
                                                       const DatabaseParserContext& context) {
        if (!yaml_has(recipe, "toolchain")) {
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
        if (value == "make") {
            return std::make_unique<MakePackageConfig>();
        }
        if (value == "meson") {
            return std::make_unique<MesonPackageConfig>();
        }
        if (value == "python") {
            return std::make_unique<PythonPackageConfig>();
        }
        fail_config(node, "recipe.toolchain", "has unsupported toolchain '" + value + "'", context);
    }

    std::vector<Override> parse_overrides(const YAML::Node& node, const std::string& path,
                                          const DatabaseParserContext& context) {
        expect_sequence(node, path, context);
        std::vector<Override> result;
        for (std::size_t i = 0; i < node.size(); ++i) {
            const std::string override_path = path + "[" + std::to_string(i) + "]";
            YAML::Node override_node        = node[i];
            check_keys(override_node, {"condition", "target", "action", "value"}, override_path,
                       context);

            Override value;
            value.condition = optional_scalar(override_node, "condition", override_path, context);
            if (value.condition.has_value()) {
                validate_condition(*value.condition, override_node["condition"],
                                   override_path + ".condition", context);
            }
            value.target = required_scalar(override_node, "target", override_path, context);
            if (yaml_has(override_node, "action")) {
                value.action =
                    parse_action(override_node["action"], override_path + ".action", context);
            }
            value.value = required_scalar(override_node, "value", override_path, context);
            result.push_back(std::move(value));
        }
        return result;
    }

    std::vector<Dependency> parse_dependencies(const YAML::Node& node, const std::string& path,
                                               const DatabaseParserContext& context) {
        expect_sequence(node, path, context);
        std::vector<Dependency> result;
        for (std::size_t i = 0; i < node.size(); ++i) {
            const std::string dep_path = path + "[" + std::to_string(i) + "]";
            const YAML::Node entry     = node[i];
            if (!entry.IsScalar()) {
                fail_config(entry, dep_path,
                            "must be a scalar; use '<name>' or '<name>@<constraints>'", context);
            }

            const std::string raw = parse_scalar(entry, dep_path, context);
            const std::size_t at  = raw.find('@');

            Dependency dep;
            if (at == std::string::npos) {
                // Simple form: just a package name (e.g. "scotch").
                dep.name = raw;
            } else {
                // "<name>@<constraints>" — e.g. "scotch@<7.0.0,>=6.0.0".
                dep.name = raw.substr(0, at);

                // Parse the comma-separated constraint list after the '@'.
                const std::string version_str = raw.substr(at + 1);
                std::size_t pos               = 0;
                const std::size_t len         = version_str.size();
                while (pos < len) {
                    // Skip leading whitespace.
                    while (pos < len &&
                           std::isspace(static_cast<unsigned char>(version_str[pos]))) {
                        ++pos;
                    }
                    if (pos >= len) break;

                    // Find the next comma.
                    const std::size_t comma    = version_str.find(',', pos);
                    const std::size_t con_end  = comma == std::string::npos ? len : comma;
                    std::string constraint_str = version_str.substr(pos, con_end - pos);

                    // Trim trailing whitespace.
                    while (!constraint_str.empty() &&
                           std::isspace(static_cast<unsigned char>(constraint_str.back()))) {
                        constraint_str.pop_back();
                    }

                    if (constraint_str.empty()) {
                        fail_config(entry, dep_path,
                                    "contains an empty constraint after '@' near position " +
                                        std::to_string(pos),
                                    context);
                    }

                    DependencyConstraint dc;
                    // Determine the operator (one of ">=", ">", "<=", "<", "==").
                    // Match the longest operator prefix first so that ">=" is not
                    // mistaken for ">" followed by "=".
                    std::size_t op_len = 0;
                    if (constraint_str.size() >= 2 &&
                        (constraint_str[0] == '>' || constraint_str[0] == '<' ||
                         constraint_str[0] == '=') &&
                        constraint_str[1] == '=') {
                        op_len = 2;
                    } else if (constraint_str.size() >= 1 &&
                               (constraint_str[0] == '>' || constraint_str[0] == '<')) {
                        op_len = 1;
                    }
                    if (op_len == 0) {
                        fail_config(entry, dep_path,
                                    "version constraint '" + constraint_str +
                                        "' is missing an operator (>=, >, <=, <, ==)",
                                    context);
                    }
                    const std::string op = constraint_str.substr(0, op_len);
                    if (op != ">=" && op != ">" && op != "<=" && op != "<" && op != "==") {
                        fail_config(entry, dep_path,
                                    "invalid version constraint operator '" + op + "'", context);
                    }
                    dc.op = op;

                    // The rest (after the operator and any whitespace) is the version value.
                    std::size_t v_start = op_len;
                    while (v_start < constraint_str.size() &&
                           std::isspace(static_cast<unsigned char>(constraint_str[v_start]))) {
                        ++v_start;
                    }
                    dc.version = constraint_str.substr(v_start);
                    if (dc.version.empty()) {
                        fail_config(entry, dep_path,
                                    "operator '" + op + "' is missing a version value", context);
                    }
                    dep.constraints.push_back(std::move(dc));

                    if (comma == std::string::npos) break;
                    pos = comma + 1;
                }
            }

            result.push_back(std::move(dep));
        }
        return result;
    }

    std::vector<Property> parse_properties(const YAML::Node& node, const std::string& path,
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
                fail_config(entry.second, property_path,
                            "must be a scalar or configurable value map", context);
            }
            result.push_back(std::move(property));
        }
        return result;
    }

}  // namespace

PackageConfigPtr parse_config_document(const YAML::Node& document,
                                       const DatabaseParserContext& context) {
    check_keys(document, {"recipe"}, "document", context);
    YAML::Node recipe = required_node(document, "recipe", "document", context);
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

    if (yaml_has(recipe, "source")) {
        config->source = parse_source(recipe["source"], "recipe.source", context);
    }
    if (yaml_has(recipe, "dependencies")) {
        config->dependencies =
            parse_dependencies(recipe["dependencies"], "recipe.dependencies", context);
    }
    if (yaml_has(recipe, "overrides")) {
        config->overrides = parse_overrides(recipe["overrides"], "recipe.overrides", context);
    }
    if (yaml_has(recipe, "build")) {
        config->build = parse_build(recipe["build"], "recipe.build", context);
    }
    if (yaml_has(recipe, "properties")) {
        config->properties = parse_properties(recipe["properties"], "recipe.properties", context);
    }
    if (yaml_has(recipe, "implementations")) {
        config->implementations =
            parse_scalar_sequence(recipe["implementations"], "recipe.implementations", context);
    }

    if (config->type == PackageType::Abstract && config->implementations.empty()) {
        fail_config(recipe, "recipe", "abstract packages require at least one implementation",
                    context);
    }

    if (config->toolchain() == Toolchain::Python) {
        if (config->type != PackageType::Package) {
            fail_config(recipe["type"], "recipe.type",
                        "must be 'package' when toolchain is 'python'", context);
        }
        if (config->name == "python") {
            fail_config(recipe["name"], "recipe.name",
                        "the Python interpreter cannot use the python toolchain", context);
        }
        if (!config->source.has_value() || config->source->type != SourceType::PyPI) {
            fail_config(recipe, "recipe.source",
                        "must use source type 'pypi' when toolchain is 'python'", context);
        }
        if (config->build.has_value()) {
            fail_config(recipe["build"], "recipe.build",
                        "is not supported for the python toolchain", context);
        }
        const bool has_python_dependency =
            std::any_of(config->dependencies.begin(), config->dependencies.end(),
                        [](const Dependency& dependency) { return dependency.name == "python"; });
        if (!has_python_dependency) {
            config->dependencies.push_back({"python", {}});
        }
    } else if (config->source.has_value() && config->source->type == SourceType::PyPI) {
        fail_config(recipe["source"], "recipe.source", "type 'pypi' requires toolchain 'python'",
                    context);
    }

    return PackageConfigPtr(std::move(config));
}

PackageConfigPtr parse_db_config(const std::filesystem::path& config_path) {
    YAML::Node document = load_yaml_file(config_path);
    return parse_config_document(document, DatabaseParserContext {config_path});
}
