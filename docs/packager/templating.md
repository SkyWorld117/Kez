# Templating

Templates allow you to define placeholders in configuration files that are replaced
with actual values at build time.

## Syntax

Template variables use `${variable_name}` syntax (brackets are mandatory, unlike bash).

```yaml
some_option: ${zlib.prefix}/lib
```

### Package Properties

Access any property declared by another package:

| Template | Resolves to |
|---|---|
| `${foo.lib}` | Raw library directory of package `foo` |
| `${foo.include}` | Raw include directory of package `foo` |
| `${foo.ldflags}` | Linker flags derived from `lib` |
| `${foo.nvldflags}` | NVIDIA linker-driver syntax flags |
| `${foo.incflags}` | Compiler `-I` flags derived from `include` |
| `${foo.prefix}` | Installation prefix of package `foo` |
| `${foo.version}` | Version of package `foo` |
| `${foo.libs}` | Link libraries of package `foo` |

### Predefined Variables

| Template | Description |
|---|---|
| `${kez.prefix}` | Root prefix for the current installation environment |
| `${kez.arch}` | Active architecture string (e.g., `x86_64`, `arm64`) |
| `${kez.arch.<variant>}` | Architecture mapped through heuristics (`heuristics/architecture.yaml`) |
| `${package.prefix}` | Prefix where the current package is installed |
| `${package.version}` | Version of the package being built |
| `${source}` | Source directory of the current package |

### Configuration Values

Access option and environment variable values:

| Template | Description |
|---|---|
| `${package.config.<option_name>}` | Resolves to the enabled/disabled value of an option |
| `${package.env.<variable_name>}` | Resolves to the value of an environment variable |

### Conditions and Templates

Template substitution is **not** performed inside `condition` strings due to syntax
ambiguity. In conditions, `package.config.variable_name` refers to the boolean state
of the option (enabled or disabled), optionally followed by a value to check against.

## Resolution Order

The template resolver (`src/uconf_parser/template_resolver.cpp`) performs substitution in
multiple passes:

1. Resolve Kez predefined variables (`kez.prefix`, `kez.arch`, etc.)
2. Resolve package properties (`package.prefix`, `package.libs`, etc.)
3. Resolve derived properties (`package.includes`, `package.ldflags`, `package.nvldflags`)
4. Resolve configuration option values (`package.config.<option_name>`)
5. Resolve environment variable values (`package.env.<variable_name>`)

Substitutions are iterative, so nested templates are resolved inside-out.
