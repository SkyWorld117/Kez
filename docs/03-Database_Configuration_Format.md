# Database Configuration Format

This document outlines the general format for configuration files used in the database. The configuration files are typically written in YAML format and are used to define various aspects of the build environment, configurations, properties etc. 


## General Structure

In principle, all concrete packages share the same structure, which is defined as follows:

```yaml
recipe:
  name: <package_name>
  description: <package_description> # Optional, but recommended
  type: <package_type>  # e.g., system, compiler, mpi, vendor, abstract, etc.
  toolchain: <toolchain_name> # Optional for system, abstract and vendor packages
  source: <source_definition> # Optional for system and abstract packages
  dependencies: <dependencies> # Only if available
  overrides: <overrides> # Optional, e.g., modify the build process for some dependencies
  build: # Optional for system, abstract and vendor packages
    preprocessing: <preprocessing_commands> # Optional, e.g., autoconf, cmake, etc.
    postprocessing: <postprocessing_commands> # Optional, e.g., manual installation, etc.
    configurations: <configuration_definition> # Optional, used to override default configuration command like `./configure` and `cmake ..`
    stages: <build_stages> # Stages for building the package (usually using make)
  properties: <properties_definition> # Optional, used to define package-specific properties
```

### `type`

It is not trivial to determine a type of a package because one package may belong to multiple types. In principle, one should determine the type of a package based on where it is supposed to be installed. For example, `intel-oneapi-mpi` is in theory both `mpi` and `vendor`, but we know that vendor packages are always the same binaries, so we intend to share this package as part of `intel-oneapi`, which should be installed to `vendors`.

### `source`

`source` describes a way for Kez to fetch the source code or prebuilt binaries. It supports various types of source files: `git`, `tarball`, `zip` and `script`.

```yaml
source:
  type: <source_type> # e.g. git, tarball, script, etc.
  url: <source_url> # Used as repository url if type is `git`
  releases:
    - version: <release_version>
      url: <release_url> # Used if type is `tarball` or `script`
      tag: <release_tag> # Used if type is `git`
    - ...
```

Notice that if the type is `script`, `url` does not have to exist. This is due to the consideration that some installation methods may already exist as a helper script under `bin/`, e.g. the NVPL installer. In such cases, package developers can ignore the `url` entry and use the script in the `preprocessing` or `postprocessing` stage instead.

### `dependencies`

`dependencies` is a list of packages. There is no version information for now, but maybe it will be added later as some packages can depend on different packages when setting to different versions.

### `overrides`

`overrides` is a list of overrides for the build process of some dependencies. It is useful when a package needs to modify the build process of its dependencies, e.g., adding additional flags or changing the installation path. The `target` entry can be any template variable as long as it is defined in the configuration file. Details can be found in the `Templating` section.

```yaml
overrides:
  - condition: <condition> # Optional, used to specify when the override should be applied
    target: <target_name> # **ANY** template variable as long as it is defined, details see `Templating` section
    action: <append|prepend|set> # Optional, default is `set`
    value: <value>
```

### `build`

#### `configurations`

A configuration blocks is defined as follows

```yaml
configurations:
  command: <configuration_command> # Optional, used to override default configuration command like `./configure` and `cmake ..`
  environment:
    - <environment_variable1>
    - <environment_variable2>
    - ...
  options:
    - <option1>
    - <option2>
    - ...
```

The `command` entry is used for `autotools` and `cmake` toolchain to override the default commands. It is ignored if not in global scope (e.g. in stages) or if the package toolchain does not match.

#### `stages`

```yaml
stages: # Stages for building the package (only make at the moment)
- target: <target_name>
  multithreaded: <boolean_value> # true or false, default is true
  configurations: <configurations_list>
- target: <target_name>
  multithreaded: <boolean_value> # true or false, default is true
  configurations: <configurations_list>
- ...
```

#### `environment`

```yaml
environment:
  - name: <variable_name>
    description: <variable_description> # Optional, but recommended
    user_configurable: <boolean_value>  # true or false, default is false
    default: <variable_value> # Optional if there is no default value, triggers an error if no condition can assign a value while default is not set
    conditions: <conditions_block> # Optional if there is default and no conditions
```

#### `options`

```yaml
options:
  - name: <option_name>
    description: <option_description> # Optional, but recommended
    user_configurable: <boolean_value> # true or false, default is false
    enabled: # Optional if it is condition independent and enabled by default
      default: <enabled_value> # true or false, optional if there is no default value, triggers an error if no condition can assign a value while default is not set
      conditions: <conditions_block> # Optional if there is default and no conditions
    enabled_format: <enabled_format> # e.g., enable-feature, default is <option_name>, discarded if enabled.default is false and not user_configurable
    disabled_format: <disabled_format> # e.g., disable-feature, default is not to inject anything into the command line
    requires: [<required_dependency1>, <required_dependency2>, ...] # Optional, takes effect if enabled is true
    enabled_value: # Optional if a value is not needed to enable the option
      default: <enabled_value> # Optional if there is no default value, triggers an error if no condition can assign a value while default is not set
      conditions: <conditions_block> # Optional if there is default and no conditions
    disabled_value: # Optional if a value is not needed to disable the option
      default: <disabled_value> # Optional if there is no default value, triggers an error if no condition can assign a value while default is not set
      conditions: <conditions_block> # Optional if there is default and no conditions
```

Option names and formats are logical names. Do not include the `--` prefix for Autotools or the
`-D` prefix for CMake; the parser adds the toolchain-specific prefix. Shell assignments such as
`CC`, `CFLAGS`, and `LDFLAGS` are not prefixed. Existing leading prefixes are accepted for custom
commands and backward compatibility.

The user-config generator supplies standard toolchain options when the database does not declare
them, and writes those options into the generated YAML as user-configurable entries backed by `${}`
templates. Autotools receives its installation prefix, compilers, dependency include flags, and
linker flags. CMake receives its install prefix, build type, compilers, language flags, linker
flags, and build/install RPATH. A database option with the same logical name takes
precedence over the generated option. Autotools `LIBS` is not generated by default; declare it
explicitly only for packages that need it.

#### `conditions`

```yaml
conditions:
  - condition: <condition>
    action: <append|prepend|set> # Optional, default is `set`
    value: <value>
  - ...
```

Actions change the behavior of the value of a variable or an option. Notice that the **ORDER** of the conditions matters. The conditions are processed from top to bottom. If there are multiple conditions with actions setting to `set` (which is the default), the value of the first condition that uses a `set` action will be used.

At last, we define the format of a `condition` using EBNF:

```ebnf
condition = 
    "environment" <variable> | 
    <option> <enabled> [<enabled_value>] | 
    <option> <disabled> [<disabled_value>] | 
    "version" <self.version><op><version>[,<op><version>] |
    <condition> && <condition> | 
    <condition> || <condition> | 
    "not" <condition> | 
    "(" <condition> ")" |
    true | false
```

### `properties`

```yaml
properties:
  parent: <parent_package>    # Optional, useful if it is a submodule of a vendor package
  prefix: <prefix>            # Optional, useful if it is a submodule of a vendor package
  c: <c_compiler>             # If compiler or mpi
  cxx: <cxx_compiler>         # If compiler or mpi
  fort: <fortran_compiler>    # If compiler or mpi
  omp_flags: <omp_flags>      # If compiler or mpi
  include: ${package.prefix}/include
  lib: ${package.prefix}/lib  # May instead be lib64 or another package-specific directory
  libs:
    default: <default_libs>
    conditions:
      - condition: <condition1>
        value: <libs1>
      - condition: <condition2>
        value: <libs2>
      - ...
  ... # More if needed
```

`include` and `lib` contain paths, not compiler flags. The parser derives `${package.includes}` and
`${package.ldflags}` when older templates still reference those names. It also derives
`${package.nvldflags}` using NVIDIA's linker-driver syntax. Explicit `includes`, `ldflags`, or
`nvldflags` properties override the derived form for packages that require exceptional flags.

For Autotools and CMake configurations, raw include/library paths from selected dependencies are
written to the generated compile/link options automatically. CMake linker flag defaults also include
dependency `libs`. Linker/RPATH syntax is selected from the package's configured compiler
(`-Wl,...` for GNU-compatible drivers and `-Xlinker ...` for the NVIDIA compiler family).

## Configuration Guidelines

As one can see, the configuration format is quite flexible and covers a wide range of use cases. However, it is also very complex and redundant if one must strictly follow the format and fill up all fields. Therefore, we recommend the following guidelines for writing configurations:

1. **Use a field only if it is necessary**: If a field is not needed for your package, you can omit it. For example, if your package does not have any specific build stages, you can skip the `stages` section.
2. **Use default values**: If a field has a default value, you can omit it in the configuration. For example, if the `cflags` field has a default value, you can skip the `conditions` section if you do not need to specify any additional flags.
3. **Use conditions wisely**: Conditions are powerful but can make the configuration more complex. In order to increase the parsing speed, the following rules **MUST** be respected:
   - Within each package, all the conditions must only depend on the previous defined entries, both in `build` as well as in `properties`.
   - Conditions in `build` should not be dependent on any non-leaf `properties` entry.
4. **Keep it simple**: If your package does not require complex configurations, keep the configuration as simple as possible. Avoid unnecessary complexity.
5. **Write descriptions**: Always provide a description for each package, environment variable, and option unless it is very obvious. This helps users understand the purpose of each configuration item. The information will also be supplied to the user when generating a customized configuration.
