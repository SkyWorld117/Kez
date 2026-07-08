# Templating

This document provides an overview of how to use templates in configuration files.

It is often difficult to write generalized configuration files that can be reused across different environments or setups without introducing dynamic elements. Templating allows you to define placeholders in your configuration files that can be replaced with actual values at the build system runtime.

## Templating Syntax

The templating syntax is bash-like, meaning a template variable is defined with `${variable_name}`. Notice that unlike bash, the brackets are mandatory in the template syntax.

Templating is mostly used to access package properties, but it can also be used to define custom variables that can be set in the configuration file.

For example, `${foo.lib}` accesses the raw library directory declared by package `foo`. The derived
templates `${foo.ldflags}`, `${foo.nvldflags}`, and `${foo.includes}` convert `lib` and `include`
paths to compiler flags. All explicitly declared properties remain directly accessible.

Exceptionally, there are also some predefined variables that can be used in the templates:

- `${kez.prefix}`: The root prefix for the current installation environment.
- `${kez.arch}` and `${kez.arch.<variant>}`: The active architecture string, optionally mapped
  through architecture heuristics (defined in `heuristics/architecture.yaml`).
- `${package.prefix}`: The prefix path where the package is installed.
- `${package.version}`: The version of the package being built.
- `${source}`: The source directory of the current package, an exception that does not follow the naming convention. If a package is supposed to be installed with a downloaded script, this will refer to the script.

Additionally, we also support reading configurations like options and variables from the configuration file. For example, if you have an option named `enable-feature`, you can access its value using `${package.config.enable-feature}`. Its value changes based on whether the option is enabled or disabled and which value is set for it. Similarly, you can access variables defined in the environment using `${package.env.variable_name}`.

### Template Resolver

The template resolver (`src/parser/template_resolver.cpp`) performs substitution in multiple passes. It:

1. Resolves Kez predefined variables (`kez.prefix`, `kez.arch`, etc.)
2. Resolves package properties (`package.prefix`, `package.libs`, etc.)
3. Resolves derived properties (`package.includes`, `package.ldflags`, `package.nvldflags`)
4. Resolves configuration option values (`package.config.<option_name>`)
5. Resolves environment variable values (`package.env.<variable_name>`)

Substitutions are performed iteratively to handle nested templates (e.g., `${foo.${bar}}` is not supported directly, but the resolver expands inner templates first).

### Conditions and Templates

Substitution is not performed in `condition` strings, as for the ease of use there exists an ambiguity in the syntax. Usually, `${package.config.variable_name}` is used to access variable values, but in a condition, it is used to define a full option, i.e., `package.config.variable_name` is the state of the option (whether it is enabled or disabled), and optionally followed by a value which the developer intends to verify. We may develop a more suitable syntax or solution in the future, but for now, it is not supported.
