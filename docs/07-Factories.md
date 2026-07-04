# Factories

This document describes the concept of factories within the system. Factories are specialized components responsible for creating and profiling configurations in batches.

Factories are a submodule of `rt` (rapid testing).

## Factory Concept

A factory is a module that performs batch instantiation of configurations and profiles the results.

```
${KEZ_WORKDIR}/factories/
 ├── factory_A/
     ├── wheels/
     ├── cellars/
     ├── tasting_room/
         ├── config.yaml
 ├── factory_B/
 ├── ...
```

- `config.yaml`: This file contains the profile (runtime configuration) for the factory.
- `wheels/`: This directory contains the user configuration files to be processed by the factory.
- `cellars/`: This directory contains the instantiated packages (cellars) based on the user configurations.
- `tasting_room/`: This directory contains the profiling results of the instantiated configurations.

## Factory Workflow

Users are expected to create a factory first with `fgr rt factory create <factory_name>`. After populating the `wheels/` directory with [user configuration files](06-User_Configuration_Format.md), one can enter the factory directory (`fgr rt factory enter <factory_name>`) and instantiate all configurations with `fgr rt build` and profile them with `fgr rt taste`.

## Runtime Profile Format

The `config.yaml` file describes the runtime configuration for each cellar in the factory. In general, it follows the following format:

```yaml
factory:
  cellars:
    - name: <cellar_name_1>
      sibling: <cellar_sibling> # (Optional) The name of another cellar, will ignore all the other fields and use the same configuration as the sibling cellar
      profiles:
      - <run_configuration_1>
      - <run_configuration_2>
      - ...
    - name: <cellar_name_2>
      sibling: <cellar_sibling>
      profiles:
      - <run_configuration_1>
      - <run_configuration_2>
      - ...
    - ...
```

Where the `run_configuration` contains the specific launch parameters for profiling:

```yaml
name: <profile_name> # A unique name for this profile. A tasting room directory with `<cellar_name>_<profile_name>` will be created to store the profiling results.
sibling: <profile_sibling> # (Optional) The name of another profile in the same cellar, will ignore all the other fields and use the same configuration as the sibling profile
inputs: <input_directory> # Files in this directory will be copied to the tasting room before running
prerun: <prerun_script> # A one-liner script to be executed before running the main command
target: <main_launch_command> # The main command to be executed, will be wrapped around with Slurm or MPI if specified
postrun: <postrun_script> # A one-liner script to be executed after running the main command
summary_regex: <regex_for_summary_extraction> # A regex pattern to extract summary information from the output log, default is empty
launcher: <none|srun|mpirun> # The launcher to be used for the main command, default is `none`
launcher_opts: <options_for_launcher> # Additional options for the launcher, default is empty
scheduler: <none|slurm> # The scheduler to be used for job submission, default is `none`
scheduler_opts: <options_for_scheduler> # Additional options for the scheduler, default is empty
num_nodes: <number_of_nodes> # Number of nodes (if applicable), default is `1`
num_procs_per_node: <number_of_processes_per_node> # Number of processes per node (if applicable), default is `1`
cores_per_proc: <number_of_cores_per_process> # Number of CPU cores per process (if applicable), default is `1`
omp_num_threads: <number_of_OpenMP_threads> # Number of OpenMP threads (if applicable), default is the value of `cores_per_proc`
gpus_per_proc: <number_of_GPUs_per_process> # Number of GPUs per process (if applicable), default is `0`
```

Since certain fields may share the same configuration, both `factory` and `cellar` levels support `inputs`, `prerun`, `target`, `postrun`, `summary_regex`, `launcher`, `launcher_opts`, `scheduler` and `scheduler_opts`. Their values can be overwritten at the lower level, but if a field is not specified at the lower level, it will inherit the value from the higher level.
