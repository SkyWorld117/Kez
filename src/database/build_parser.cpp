#include <database/build_parser.hpp>
#include <database/condition_parser.hpp>
#include <database/parser_utils.hpp>
#include <database/source_parser.hpp>
#include <unordered_set>
#include <utility>
#include <utils/yaml_utils.hpp>

namespace {
    template <typename T, typename ValueParser>
    ConfigurableValue<T> parse_configurable(const YAML::Node& node, const std::string& path,
                                            const DatabaseParserContext& context,
                                            ValueParser parse_value) {
        expect_map(node, path, context);
        check_keys(node, {"default", "conditions"}, path, context);
        ConfigurableValue<T> result;
        if (yaml_has(node, "default") && !node["default"].IsNull()) {
            result.default_value = parse_value(node["default"], path + ".default");
        }
        if (yaml_has(node, "conditions")) {
            YAML::Node conditions = node["conditions"];
            expect_sequence(conditions, path + ".conditions", context);
            for (std::size_t i = 0; i < conditions.size(); ++i) {
                const std::string condition_path = path + ".conditions[" + std::to_string(i) + "]";
                YAML::Node condition_node        = conditions[i];
                expect_map(condition_node, condition_path, context);
                check_keys(condition_node, {"condition", "action", "value"}, condition_path,
                           context);

                ConditionalValue<T> condition;
                condition.condition =
                    required_scalar(condition_node, "condition", condition_path, context);
                validate_condition(condition.condition, condition_node["condition"],
                                   condition_path + ".condition", context);
                if (yaml_has(condition_node, "action")) {
                    condition.action =
                        parse_action(condition_node["action"], condition_path + ".action", context);
                }
                condition.value =
                    parse_value(required_node(condition_node, "value", condition_path, context),
                                condition_path + ".value");
                result.conditions.push_back(std::move(condition));
            }
        }
        if (!result.default_value.has_value() && result.conditions.empty()) {
            fail_config(node, path, "must define a default or at least one condition", context);
        }
        return result;
    }

}  // namespace

ConfigurableValue<bool> parse_bool_configurable(const YAML::Node& node, const std::string& path,
                                                const DatabaseParserContext& context) {
    ConfigurableValue<bool> result = parse_configurable<bool>(
        node, path, context, [&context](const YAML::Node& value, const std::string& value_path) {
            return parse_boolean(value, value_path, context);
        });
    for (const auto& condition : result.conditions) {
        if (condition.action != ValueAction::Set) {
            fail_config(node, path, "boolean conditions only support the set action", context);
        }
    }
    return result;
}

ConfigurableValue<std::string> parse_string_configurable(const YAML::Node& node,
                                                         const std::string& path,
                                                         const DatabaseParserContext& context) {
    return parse_configurable<std::string>(
        node, path, context, [&context](const YAML::Node& value, const std::string& value_path) {
            return parse_scalar(value, value_path, context, true);
        });
}

namespace {
    EnvironmentVariable parse_environment_variable(const YAML::Node& node, const std::string& path,
                                                   const DatabaseParserContext& context) {
        expect_map(node, path, context);
        check_keys(
            node, {"name", "description", "user_configurable", "requires", "default", "conditions"},
            path, context);

        EnvironmentVariable result;
        result.name        = required_scalar(node, "name", path, context);
        result.description = optional_scalar(node, "description", path, context);
        if (yaml_has(node, "user_configurable")) {
            result.user_configurable =
                parse_boolean(node["user_configurable"], path + ".user_configurable", context);
        }
        if (yaml_has(node, "requires")) {
            result.
                requires
            = parse_scalar_sequence(node["requires"], path + ".requires", context);
        }
        if (yaml_has(node, "default") && !node["default"].IsNull()) {
            result.value.default_value = parse_scalar(node["default"], path + ".default", context);
        }
        if (yaml_has(node, "conditions")) {
            YAML::Node wrapper(YAML::NodeType::Map);
            if (yaml_has(node, "default")) {
                wrapper["default"] = node["default"];
            }
            wrapper["conditions"] = node["conditions"];
            result.value          = parse_string_configurable(wrapper, path, context);
        }
        return result;
    }

    BuildOption parse_option(const YAML::Node& node, const std::string& path,
                             const DatabaseParserContext& context) {
        expect_map(node, path, context);
        check_keys(node,
                   {"name", "description", "user_configurable", "enabled", "enabled_format",
                    "disabled_format", "requires", "enabled_value", "disabled_value"},
                   path, context);

        BuildOption result;
        result.name        = required_scalar(node, "name", path, context);
        result.description = optional_scalar(node, "description", path, context);
        if (yaml_has(node, "user_configurable")) {
            result.user_configurable =
                parse_boolean(node["user_configurable"], path + ".user_configurable", context);
        }
        if (yaml_has(node, "enabled")) {
            result.enabled = parse_bool_configurable(node["enabled"], path + ".enabled", context);
        }
        result.enabled_format  = optional_scalar(node, "enabled_format", path, context);
        result.disabled_format = optional_scalar(node, "disabled_format", path, context);
        if (yaml_has(node, "requires")) {
            result.
                requires
            = parse_scalar_sequence(node["requires"], path + ".requires", context);
        }
        if (yaml_has(node, "enabled_value")) {
            result.enabled_value =
                parse_string_configurable(node["enabled_value"], path + ".enabled_value", context);
        }
        if (yaml_has(node, "disabled_value")) {
            result.disabled_value = parse_string_configurable(node["disabled_value"],
                                                              path + ".disabled_value", context);
        }
        return result;
    }
}  // namespace

BuildConfiguration parse_build_configuration(const YAML::Node& node, const std::string& path,
                                             const DatabaseParserContext& context) {
    expect_map(node, path, context);
    check_keys(node, {"command", "environment", "options"}, path, context);

    BuildConfiguration result;
    result.command = optional_scalar(node, "command", path, context);
    if (yaml_has(node, "environment")) {
        YAML::Node environment = node["environment"];
        expect_sequence(environment, path + ".environment", context);
        for (std::size_t i = 0; i < environment.size(); ++i) {
            EnvironmentVariable variable = parse_environment_variable(
                environment[i], path + ".environment[" + std::to_string(i) + "]", context);
            result.environment.push_back(std::move(variable));
        }
    }
    if (yaml_has(node, "options")) {
        YAML::Node options = node["options"];
        expect_sequence(options, path + ".options", context);
        std::unordered_set<std::string> names;
        for (std::size_t i = 0; i < options.size(); ++i) {
            BuildOption option =
                parse_option(options[i], path + ".options[" + std::to_string(i) + "]", context);
            if (!names.emplace(option.name).second) {
                warn_config(options[i], path + ".options",
                            "contains duplicate option name '" + option.name +
                                "'; both entries are retained",
                            context);
            }
            result.options.push_back(std::move(option));
        }
    }
    return result;
}

Build parse_build(const YAML::Node& node, const std::string& path,
                  const DatabaseParserContext& context) {
    expect_map(node, path, context);
    check_keys(node, {"preprocessing", "postprocessing", "configurations", "stages"}, path,
               context);

    Build result;
    result.preprocessing  = optional_scalar(node, "preprocessing", path, context);
    result.postprocessing = optional_scalar(node, "postprocessing", path, context);
    if (yaml_has(node, "configurations")) {
        result.configurations =
            parse_build_configuration(node["configurations"], path + ".configurations", context);
    }
    if (yaml_has(node, "stages")) {
        YAML::Node stages = node["stages"];
        expect_sequence(stages, path + ".stages", context);
        for (std::size_t i = 0; i < stages.size(); ++i) {
            const std::string stage_path = path + ".stages[" + std::to_string(i) + "]";
            YAML::Node stage_node        = stages[i];
            expect_map(stage_node, stage_path, context);
            check_keys(stage_node, {"target", "multithreaded", "configurations"}, stage_path,
                       context);

            YAML::Node target = required_node(stage_node, "target", stage_path, context);
            BuildStage stage;
            if (!target.IsNull()) {
                stage.target = parse_scalar(target, stage_path + ".target", context);
            }
            if (yaml_has(stage_node, "multithreaded")) {
                stage.multithreaded = parse_boolean(stage_node["multithreaded"],
                                                    stage_path + ".multithreaded", context);
            }
            if (yaml_has(stage_node, "configurations")) {
                stage.configurations = parse_build_configuration(
                    stage_node["configurations"], stage_path + ".configurations", context);
            }
            result.stages.push_back(std::move(stage));
        }
    }
    return result;
}
