# General Configuration Format

This document outlines the general format for configuration files used in our system. The configuration files are typically written in YAML format and are used to define various aspects of the environment, including compilers, MPI implementations, and other utilities.


## General Structure

In principle, all concrete packages share the same structure, which is defined as follows:

```yaml
cheese:
  name: <package_name>
  description: <package_description> # Optional, but recommended
  type: <package_type>  # e.g., compiler, mpi, vendor, abstract, etc.
  toolchain: <toolchain_name> # if not abstract or vendor package
  source:
    type: <source_type> # e.g., git, archive, etc.
    url: <source_url>   # If source type is git, this is the repository URL
    releases:
        - <release1>
        - <release2>
        - ...
  dependencies: # Not needed if there are no dependencies
    - <dependency1>
    - <dependency2>
    - ...
  build:
    preprocessing: <preprocessing_commands> # Optional, e.g., autoconf, cmake, etc.
    postprocessing: <postprocessing_commands> # Optional, e.g., manual installation, etc.
    configurations: # Configuration stage for autotools and cmake
      environment:
        - <environment_variable1>
        - <environment_variable2>
        - ...
      options:
        - <option1>
        - <option2>
        - ...
    stages: # Stages for building the package (usually using make)
      - target: <target_name>
        multithreaded: <boolean_value> # true or false, default is true
        configurations: <configurations_list>
      - target: <target_name>
        multithreaded: <boolean_value> # true or false, default is true
        configurations: <configurations_list>
      - ...
  properties:
    c: <c_compiler>             # If compiler or mpi
    cxx: <cxx_compiler>         # If compiler or mpi
    fort: <fortran_compiler>    # If compiler or mpi
    omp_flags: <omp_flags>      # If compiler or mpi
    cflags:
      default: <default_cflags>
      conditions:
        - condition: <condition1>
          value: <flags1>
        - condition: <condition2>
          value: <flags2>
        - ...
    cxxflags:
      default: <default_cxxflags>
      conditions:
        - condition: <condition1>
          value: <flags1>
        - condition: <condition2>
          value: <flags2>
        - ...
    cppflags:
      default: <default_cppflags>
      conditions:
        - condition: <condition1>
          value: <flags1>
        - condition: <condition2>
          value: <flags2>
        - ...
    fortflags:
      default: <default_fortflags>
      conditions:
        - condition: <condition1>
          value: <flags1>
        - condition: <condition2>
          value: <flags2>
        - ...
    ldflags:
      default: <default_ldflags>
      conditions:
        - condition: <condition1>
          value: <flags1>
        - condition: <condition2>
          value: <flags2>
        - ...
    libs:
      default: <default_libs>
      conditions:
        - condition: <condition1>
          value: <libs1>
        - condition: <condition2>
          value: <libs2>
        - ...
    includes:
      default: <default_includes>
      conditions:
        - condition: <condition1>
          value: <includes1>
        - condition: <condition2>
          value: <includes2>
        - ...
```

### Source

Each release depends on the type of the source. For example, if the source type is `git`, the `releases` section should contain the tags or branches that are available in the repository. If the source type is an archive, it should contain the URLs to the archives.

```yaml
source:
  type: git
  url: <repository_url>
  releases:
    - version: 1.0.0
      tag: v1.0.0
    - version: 1.1.0
      tag: v1.1.0
    - version: 2.0.0
      tag: v2.0.0
    - ...

source:
  type: tarball
  url: <archive_url> # Optional
  releases:
    - version: 1.0.0
      url: <archive_url_1.0.0>
    - version: 1.1.0
      url: <archive_url_1.1.0>
    - version: 2.0.0
      url: <archive_url_2.0.0>
    - ...
```

### Dependencies

We now define the format of the environment variables and options in the `build` section:

```yaml
environment:
  - name: <variable_name>
    description: <variable_description> # Optional, but recommended
    user_configurable: <boolean_value>  # true or false, default is false
    default: <variable_value> # Optional if there is no default value, triggers an error if no condition can assign a value while default is not set
    conditions: # Optional if there is default and no conditions
      - condition: <condition1>
        value: <value1>
      - condition: <condition2>
        value: <value2>
      - ...

options:
  - name: <option_name>
    description: <option_description> # Optional, but recommended
    user_configurable: <boolean_value> # true or false, default is false
    enabled: # Optional if it is condition independent and enabled by default
      default: <enabled_value> # true or false, optional if there is no default value, triggers an error if no condition can assign a value while default is not set
      conditions: # Optional if there is default and no conditions
        - condition: <condition1>
          value: <value1> # boolean, e.g., true or false
        - condition: <condition2>
          value: <value2> # boolean, e.g., true or false
        - ...
    enabled_format: <enabled_format> # e.g., --enable-feature, default is <option_name>, discarded if enabled.default is false and not user_configurable
    disabled_format: <disabled_format> # e.g., --disable-feature, default is not to inject anything into the command line
    requires: [<required_dependency1>, <required_dependency2>, ...] # Optional, takes effect if enabled is true
    enabled_value: # Optional if a value is not needed to enable the option
      default: <enabled_value> # Optional if there is no default value, triggers an error if no condition can assign a value while default is not set
      conditions: # Optional if there is default and no conditions
        - condition: <condition1>
          value: <value1>
        - condition: <condition2>
          value: <value2>
        - ...
    disabled_value: # Optional if a value is not needed to disable the option
      default: <disabled_value> # Optional if there is no default value, triggers an error if no condition can assign a value while default is not set
      conditions: # Optional if there is default and no conditions
        - condition: <condition1>
          value: <value1>
        - condition: <condition2>
          value: <value2>
        - ...
```

### Conditions

At last, we define the format of a `condition` using EBNF:

```ebnf
condition = 
    "required" <dependency> | 
    "environment" <variable> | 
    <option> <enabled> [<enabled_value>] | 
    <option> <disabled> [<disabled_value>] | 
    "version" <version_range> |
    <condition> && <condition> | 
    <condition> || <condition> | 
    "not" <condition> | 
    "(" <condition> ")"
```


## Configuration Guidelines

As one can see, the configuration format is quite flexible and covers a wide range of use cases. However, it is also very complex and redundant if one must strictly follow the format and fill up all fields. Therefore, we recommend the following guidelines for writing configurations:

1. **Use a field only if it is necessary**: If a field is not needed for your package, you can omit it. For example, if your package does not have any specific build stages, you can skip the `stages` section.
2. **Use default values**: If a field has a default value, you can omit it in the configuration. For example, if the `cflags` field has a default value, you can skip the `conditions` section if you do not need to specify any additional flags.
3. **Use conditions wisely**: Conditions are powerful but can make the configuration more complex.
4. **Keep it simple**: If your package does not require complex configurations, keep the configuration as simple as possible. Avoid unnecessary complexity.
5. **Write descriptions**: Always provide a description for each package, environment variable, and option unless it is very obvious. This helps users understand the purpose of each configuration item. The information will also be supplied to the user when generating a customized configuration.