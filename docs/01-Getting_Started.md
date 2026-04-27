# Getting Started

Fromager is supposed to run on Linux systems. It heavily relies on the GNU toolchain. As a minimum requirement, you need:

- GNU/Linux
- bash
- make
- wget
- curl
- git
- jq
- tar
- gettext (autopoint)

Since Fromager is an HPC-focused package manager, it is designed to work seamlessly with HPC Linux distributions such as CentOS, Rocky Linux, and RHEL. It is tested on Rocky Linux. 

## Download

You can download Fromager from the [GitHub repository](https://github.com/SkyWorld117/Fromager).

```bash
git clone -c feature.manyFiles=true git@github.com:SkyWorld117/Fromager.git
```

After cloning the repository, you can place it anywhere you like, including your home directory. The whole project is not large and can be easily managed. However, it does contain many files since the whole database is included. If your cluster has a quota limit of the number of files, you may need to clone the repository elsewhere.

## Initialization of Fromager Environment

Fromager requires a directory to store its configurations and cellars (which contain all the packages you install). You can specify it using the environment variable `FROMAGER_WORKDIR`. 

For example, it can be a good choice to set it to a directory with ample space, such as the scratch filesystem:
```bash
export FROMAGER_WORKDIR=/scratch/${USER}/.fromager
```

After setting the `FROMAGER_WORKDIR`, you can initialize the Fromager environment by running the following command:

```bash
cd Fromager
source setup-env.sh
```

For the first time you run `setup-env.sh`, you will be asked to configure the configuration file listed in the next section. After that, you can simply run `source setup-env.sh` again to load the Fromager environment variables and functions. You can find example configuration ready to be copied to your `config.yaml` in Fromager/configs/

It is recommended to add both of the commands to your `.bashrc`. 

## Configuring

The `setup-env.sh` script will create `$FROMAGER_WORKDIR/config.yaml` which one can customize certain behaviors of Fromager.

It looks like this by default:

```yaml
fromager:
  n_proc_for_build: 4

  default_compiler: system

  paths:
    cellars: cellars
    system: cellars/system
    utilities: cellars/utilities
    compilers: cellars/compilers
    mpis: cellars/mpis
    vendors: cellars/vendors
    factories: factories

  external:
    rdma-core:
      prefix: ~
      version: ~
    hcoll:
      prefix: ~
      version: ~
    gdrcopy:
      prefix: ~
      version: ~
    slurm:
      prefix: ~
      version: ~
```

`n_proc_for_build` specifies the number of processes to use for building packages. You can change it to match your system's capabilities. This is used unless a package requires explicitly not to be built with multiple processes. We will talk about this more in the [`stages`](03-Database_Configuration_Format.md#stages) section. 

`default_compiler` specifies the default compiler to use when building packages. It can be set to `system` or a specific compiler in the `compilers` cellar in the format of `<vendor>@<version>`. For example, if you have a GCC 11.4.0 installed in the `compilers` cellar, you can set it to `gcc@11.4.0`.

`paths` specifies the directory structure of the cellar. They are relative paths to the `FROMAGER_WORKDIR`. You can change them if you want.

`external` contains a list of external dependencies that Fromager is supposed to use. These are usually libraries that are not supposed to be built by the users of a cluster, as they may cause ABI incompatibilities if the package versions mismatch the driver versions. Fromager will not use them if the entries are left as `~`, which means "null". 

Users are supposed to find the packages installed by their system administrators or contact them if certain packages are not available.

`prefix` specifies the installation prefix of the external package, while `version` specifies the version of the package. Here is the example configuration on the Piora cluster:

```yaml
fromager:
  n_proc_for_build: 32

  external:
    rdma-core:
      prefix: /usr
      version: 51.0
    hcoll:
      prefix: /opt/mellanox/hcoll
      version: 4.8.3227
    gdrcopy:
      prefix: ~
      version: ~
```

## Initialization of Fromager Toolchain

To make Fromager as distribution-independent as possible, it creates a mini toolchain which contains a GCC compiler, GNU build system, cmake set, and other essential tools. This toolchain is isolated from the system environment and can be easily managed within the Fromager environment.

This process takes however quite some time and compute power, so it is recommended to run it on a dedicated compute node.

You can initialize the toolchain by running the following command:

```bash
fgr init
```

It is recommended to run this command on the login node or a compute node with full compiling toolchain installed. The essential packages are listed above.

## Usage

You can find all available commands and their documentation by running

```bash
fgr --help
```

### Logic and Terminologies

We first introduce the non-conventional terminologies:

- Fromager, or in commands as `fgr` for short, refers to the package manager
- Cellar, an isolated folder that contains the target packages
- Cheese, a configuration file for a package, appears as the header in user configuration files and database configuration files

In HPC workflows it is common to need multiple versions or build configurations of the same application side by side. Fromager achieves this by placing each application and its dependencies into an isolated directory called a **cellar**. Cellars act like self-contained prefixes — similar to Python virtual environments — so different applications never interfere with each other.

## Basic Usage

The typical workflow for installing an application is:

```bash
# 1. Generate a user configuration template for the package
fgr template conquest --cellar myapp

# 2. Edit the generated YAML to select versions and options, then install
fgr install -r examples/conquest.yaml --cellar myapp

# 3. Enter the cellar to use the installed application
fgr cellar enter myapp

# 4. Run your application
conquest ...

# 5. Exit the cellar when done
fgr cellar exit
```

For compilers and MPI implementations, you install them to their shared cellars (no `--cellar` flag needed) and then load them into your shell:

```bash
# Install GCC 13.2.0
fgr install gcc --config version=13.2.0

# Load it into the current shell
fgr compiler load gcc-13.2.0

# Install OpenMPI against the loaded compiler
fgr install -r examples/openmpi.yaml

# Load it
fgr mpi load openmpi-5.0.9-gcc-13.2.0
```

See [CLI Reference](08-CLI_Reference.md) for the full command documentation.

## Uninstalling and Reinstalling Fromager

To uninstall Fromager, you can simply remove the cloned repository and the `FROMAGER_WORKDIR`:

```bash
rm -rf Fromager
rm -rf $FROMAGER_WORKDIR
```

You may want to reload the shell after that to remove the environment variables set by `setup-env.sh`.

To reinstall Fromager, you can simply clone the repository again and run `setup-env.sh` again, following the instructions above.