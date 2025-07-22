# Templating

This document provides an overview of how to use templates in configuration files. 

It is often difficult to write generalized configuration files that can be reused across different environments or setups without introducing dynamic elements. Templating allows you to define placeholders in your configuration files that can be replaced with actual values at the build system runtime. 

## Templating Syntax

The templating syntax is bash-like, meaning a template variable is defined with `${variable_name}`. Notice that unlike bash, the brackets are mandatory in the template syntax.

Templating is mostly used to access package properties, but it can also be used to define custom variables that can be set in the configuration file.

For example, one wants to access the `ldflags` property of a package named `foo`, one can use `${foo.ldflags}`, as long as it is well defined in the configuration file. All the properties in the `properties` section can be accessed in this way.

Exceptionally, there are also some predefined variables that can be used in the templates:

- `${package.prefix}`: The prefix path where the package is installed.
- `${package.version}`: The version of the package being built.
- `${source}`: The source directory of the current package, an exception that does not follow the naming convention.

Additionally, we also support reading configurations like options and variables from the configuration file. For example, if you have an option named `enable-feature`, you can access its value using `${package.config.enable-feature}`. Its value changes based on whether the option is enabled or disabled and which value is set for it. Similarly, you can access variables defined in the environment using `${package.config.variable_name}`.

## TODOs

- [ ] Allow using self-reference in templates, e.g. `${self.prefix}` to refer to the current package's prefix.