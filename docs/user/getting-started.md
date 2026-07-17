# Getting Started

Kez is supposed to run on Linux systems. It heavily relies on the GNU toolchain. As a minimum requirement, you need:

- GNU/Linux
- bash
- make
- wget
- curl
- git
- jq
- tar
- gettext (autopoint)

Since Kez is an HPC-focused package manager, it is designed to work seamlessly with HPC Linux distributions such as CentOS, Rocky Linux, and RHEL. It is tested on Rocky Linux.

## Download

You can download Kez from the [GitHub repository](https://github.com/SkyWorld117/Kez).

```bash
git clone -c feature.manyFiles=true https://github.com/SkyWorld117/Kez.git
```

After cloning the repository, you can place it anywhere you like, including your home directory. The whole project is not large and can be easily managed. However, it does contain many files since the whole database is included. If your cluster has a quota limit of the number of files, you may need to clone the repository elsewhere.

## Initialization of Kez Environment

Kez requires a directory to store its configurations and environments (which contain all the packages you install). You can specify it using the environment variable `KEZ_WORKDIR`.

For example, it can be a good choice to set it to a directory with ample space, such as the scratch filesystem:
```bash
export KEZ_WORKDIR=/scratch/${USER}/.kez
```

After setting the `KEZ_WORKDIR`, you can initialize the Kez environment by running the following command:

```bash
cd Kez
source setup-env.sh
```

For the first time you run `setup-env.sh`, you will be asked to configure the configuration file listed in the next section. After that, you can simply run `source setup-env.sh` again to load the Kez environment variables and functions.

It is recommended to add both of the commands to your `.bashrc`.

## Configuring

The `setup-env.sh` script will create `$KEZ_WORKDIR/config.yaml` which one can customize certain behaviors of Kez.

It looks like this by default:

```yaml
settings:
  n_proc_for_build: 4
  max_n_jobs_for_build: 2

  default_compiler: system

  external:
    rdma-core:
      prefix:
      version:
    gdrcopy:
      prefix:
      version:
    slurm:
      prefix:
      version:
```

`n_proc_for_build` specifies the number of processes to use for building each package (e.g. `make -jN`). You can change it to match your system's capabilities. This is used unless a package requires explicitly not to be built with multiple processes.

`max_n_jobs_for_build` specifies the maximum number of packages to build **in parallel** during installation. For example, with `n_proc_for_build: 32` and `max_n_jobs_for_build: 8`, up to eight packages are dispatched concurrently, each using up to 32 processors. These settings are exported as `KEZ_NPROC` and `KEZ_NJOBS` respectively after sourcing `setup-env.sh`.

`default_compiler` specifies the default compiler to use when building packages. It can be set to `system` or a specific compiler in the `compilers` environment in the format of `<vendor>@<version>`. For example, if you have a GCC 11.4.0 installed in the `compilers` environment, you can set it to `gcc@11.4.0`.

`external` contains a list of external dependencies that Kez is supposed to use. These are usually libraries that are not supposed to be built by the users of a cluster, as they may cause ABI incompatibilities if the package versions mismatch the driver versions. Kez will not use them if the entries are left empty (null).

Users are supposed to find the packages installed by their system administrators or contact them if certain packages are not available.

`prefix` specifies the installation prefix of the external package, while `version` specifies the version of the package. Here is the example configuration on the Piora cluster:

```yaml
settings:
  n_proc_for_build: 32
  max_n_jobs_for_build: 8

  external:
    rdma-core:
      prefix: /usr
      version: 51.0
    hcoll:
      prefix: /opt/mellanox/hcoll
      version: 4.8.3227
    gdrcopy:
      prefix:
      version:
```

## Initialization of Kez Toolchain

To make Kez as distribution-independent as possible, it creates a mini toolchain which contains a GCC compiler, GNU build system, cmake set, and other essential tools. This toolchain is isolated from the system environment and can be easily managed within the Kez environment.

This process takes quite some time and compute power, so it is recommended to run it on a dedicated compute node.

You can initialize the toolchain by running the following command:

```bash
kez init
```

You can also initialize using a distro-provided compiler instead of building one from scratch:

```bash
kez init --use-distro-compiler
```

In this case the distro compiler will be used for all further package installations. It is highly recommended **NOT** to use this option unless you are sure that the distro compiler is compatible with all the packages you want to install, or you can choose to install another compiler later and use that compiler as the default one.

It is recommended to run this command on the login node or a compute node with full compiling toolchain installed. The essential packages are listed in `manifest.yaml`.

## Usage

You can find all available commands and their documentation by running

```bash
kez --help
```

See the [CLI Reference](cli-reference.md) for a complete list of commands.

### Logic and Terminologies

- **Kez**: The package manager (the `kez` command).
- **Environment**: An isolated folder that contains the target packages.
- **Recipe**: A configuration file for a package, appears as the root key in user configuration files and database configuration files.

## Uninstalling and Reinstalling Kez

To uninstall Kez, you can simply remove the cloned repository and the `KEZ_WORKDIR`:

```bash
rm -rf Kez
rm -rf $KEZ_WORKDIR
```

You may want to reload the shell after that to remove the environment variables set by `setup-env.sh`.

To reinstall Kez, you can simply clone the repository again and run `setup-env.sh` again, following the instructions above.
