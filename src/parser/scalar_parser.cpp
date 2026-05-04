#include <parser/scalar_parser.hpp>

std::string parse_scalar(const std::string& scalar_str, ParserContext& context) {
    auto resolver = [&](const std::string& template_name) {
        return parse_template(template_name, context);
    };
    return resolve_templates_in_scalar(scalar_str, resolver, "variable");
}
