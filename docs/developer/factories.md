# Factories

Factories are specialized components for batch building and profiling of multiple
package configurations.

## Concept

A factory directory contains recipe YAML files, build outputs (buildspace), and
profiling results (runspace).

```
${KEZ_WORKDIR}/factories/
 ├── factory_A/
     ├── recipes/       # User configuration YAMLs (one per build variant)
     ├── buildspace/    # Installed package prefixes
     ├── runspace/      # Profiling results
         ├── config.yaml  # Runtime profile definitions
     ├── factory_B/
     ├── ...
```

## Workflow

```bash
# Create a factory
kez factory create my-factory

# Populate with recipes
cp my-configs/*.yaml $KEZ_WORKDIR/factories/my-factory/recipes/

# Enter the factory context
kez factory enter my-factory

# Build all recipes
kez factory build

# Run profiles
kez factory run

# Extract summary metrics from output
kez factory summarize

# Leave the factory context
kez factory exit
```

## Subcommands

| Subcommand | Description |
|---|---|
| `create NAME` | Create a new factory with recipes, buildspace, and runspace directories |
| `remove NAME` | Delete a factory and all its data |
| `list` | List all factories |
| `enter NAME` | Set `KEZ_FACTORY` in the current shell |
| `exit` | Unset `KEZ_FACTORY` |
| `which` | Show the current factory |
| `build` | Build all recipe YAML files in `recipes/` |
| `run` | Execute profiles defined in `runspace/config.yaml` |
| `summarize` | Print lines matching profile summary regexes |

### Build Options

| Option | Description |
|---|---|
| `-d, --dry-run` | Show installation commands without executing |
| `-f, --force` | Reinstall packages already recorded in state.yaml |
| `-S, --with-slurm` | Run installation through sbatch |

## Runtime Profile Format

The runtime configuration (`runspace/config.yaml`) defines how each buildspace is profiled:

```yaml
factory:
  buildspace:
    - name: <build_name_1>
      sibling: <build_sibling>        # Inherit all fields from another buildspace
      profiles:
        - name: <profile_name>
          inputs: <input_directory>            # Copied to runspace before run
          prerun: <prerun_script>              # Executed before main command
          target: <main_launch_command>         # The command to profile
          postrun: <postrun_script>             # Executed after main command
          summary_regex: <regex>                # Extract from output logs
          launcher: none|srun|mpirun            # Launcher wrapper
          launcher_opts: <options>
          scheduler: none|slurm                 # Scheduler wrapper
          scheduler_opts: <options>
          num_nodes: <number>
          num_procs_per_node: <number>
          cores_per_proc: <number>
          omp_num_threads: <number>             # Default: cores_per_proc
          gpus_per_proc: <number>               # Default: 0
    - name: <build_name_2>
      ...
```

### Multi-level Inheritance

The fields `inputs`, `prerun`, `target`, `postrun`, `summary_regex`, `launcher`,
`launcher_opts`, `scheduler`, and `scheduler_opts` can be defined at three levels:

1. **Factory level** (top of the YAML, outside any buildspace)
2. **Buildspace level** (inside a buildspace entry)
3. **Profile level** (inside a profile entry)

A lower level overrides the higher level. The `${key}` pattern in a value will be
replaced with the inherited value from the parent level.

### Sibling Resolution

Both buildspaces and profiles support `sibling` references. A sibling inherits the
entire configuration of the named peer. Cycle detection prevents infinite recursion.
