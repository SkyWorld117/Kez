#pragma once

#include <yaml-cpp/yaml.h>

#include <string>

// Assign a scalar below the supplied node. Map keys are separated by dots. When a path
// enters a sequence, an element can be selected by its numeric index or by the value of
// its `name` or `target` field.
void traverse(const std::string& path, const std::string& value, YAML::Node node);
