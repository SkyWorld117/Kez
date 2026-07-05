#include <cctype>
#include <cmdline_parser/traverse.hpp>
#include <cstdlib>
#include <limits>
#include <string>
#include <utils/colored_io.hpp>
#include <utils/yaml_utils.hpp>
#include <vector>

namespace {
    [[noreturn]] void invalid_path(const std::string& path) {
        ERROR("Invalid configuration path: " + path);
        exit(EXIT_FAILURE);
    }

    std::vector<std::string> split_path(const std::string& path) {
        if (path.empty()) {
            invalid_path(path);
        }

        std::vector<std::string> result;
        std::size_t begin = 0;
        while (begin <= path.size()) {
            const std::size_t end  = path.find('.', begin);
            const std::string part = path.substr(begin, end - begin);
            if (part.empty()) {
                invalid_path(path);
            }
            result.push_back(part);
            if (end == std::string::npos) {
                break;
            }
            begin = end + 1;
        }
        return result;
    }

    bool numeric_index(const std::string& value, std::size_t& result) {
        if (value.empty()) {
            return false;
        }
        std::size_t parsed = 0;
        for (const char character : value) {
            if (!std::isdigit(static_cast<unsigned char>(character))) {
                return false;
            }
            const std::size_t digit = static_cast<std::size_t>(character - '0');
            if (parsed > (std::numeric_limits<std::size_t>::max() - digit) / 10) {
                return false;
            }
            parsed = parsed * 10 + digit;
        }
        result = parsed;
        return true;
    }

    std::size_t sequence_index(const YAML::Node& node, const std::string& selector,
                               const std::string& path) {
        std::size_t index = 0;
        if (numeric_index(selector, index)) {
            if (index >= node.size()) {
                invalid_path(path);
            }
            return index;
        }

        for (std::size_t current = 0; current < node.size(); ++current) {
            const YAML::Node item = node[current];
            if (!item.IsMap()) {
                continue;
            }
            for (const std::string key : {"name", "target"}) {
                if (yaml_has(item, key) && item[key].IsScalar() && item[key].Scalar() == selector) {
                    return current;
                }
            }
        }
        invalid_path(path);
    }

    void assign(const std::vector<std::string>& parts, std::size_t position,
                const std::string& value, YAML::Node node, const std::string& full_path) {
        if (node.IsMap()) {
            const std::string& key = parts[position];
            if (!node[key].IsDefined()) {
                invalid_path(full_path);
            }
            if (position + 1 == parts.size()) {
                node[key] = value;
                return;
            }
            assign(parts, position + 1, value, node[key], full_path);
            return;
        }

        if (node.IsSequence()) {
            const std::size_t index = sequence_index(node, parts[position], full_path);
            if (position + 1 == parts.size()) {
                node[index] = value;
                return;
            }
            assign(parts, position + 1, value, node[index], full_path);
            return;
        }

        invalid_path(full_path);
    }
}  // namespace

void traverse(const std::string& path, const std::string& value, YAML::Node node) {
    const std::vector<std::string> parts = split_path(path);
    assign(parts, 0, value, node, path);
}
