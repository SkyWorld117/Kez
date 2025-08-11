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

Since Fromager is an HPC-focused package manager, it is designed to work seamlessly with HPC Linux distributions such as CentOS, Rocky Linux, and RHEL. It is tested on Rocky Linux. 

## Download

You can download Fromager from the [GitHub repository](https://github.com/SkyWorld117/Fromager).

```bash
git clone -c feature.manyFiles=true https://github.com/SkyWorld117/Fromager.git
```

After cloning the repository, you can place it anywhere you like, including your home directory. The whole project is not large and can be easily managed. However, it does contain many files since the whole database is included. If your cluster has a quota limit of the number of files, you may need to clone the repository elsewhere.

## Initialization of Fromager Environment

Fromager requires a directory to store its configurations and cellars (which contain all the packages you install). You can specify it using the environment variable `FROMAGER_WORKDIR`. 

For example, it can be a good choice to set it to a directory with ample space, such as the scratch filesystem:
```bash
export FROMAGER_WORKDIR=/scratch/${USER}/.fromager
```

If not specified, Fromager will use the default directory `~/.fromager`.

After setting the `FROMAGER_WORKDIR`, you can initialize the Fromager environment by running the following command:

```bash
cd Fromager
source setup-env.sh
```

It is recommended to add both of the commands to your `.bashrc`. 

## Configuring

The `setup-env.sh` script will create `$FROMAGER_WORKDIR/config.yaml` which one can customize certain behaviors of Fromager.

It looks like this by default:

```yaml
fromager:
  n_proc_for_build: 4

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
```

`n_proc_for_build` specifies the number of processes to use for building packages. You can change it to match your system's capabilities. This is used unless a package requires explicitly not to be built with multiple processes. We will talk about this more in the #TODO section. 

`external` contains a list of external dependencies that Fromager is supposed to use. These are usually libraries that are not supposed to be built by the users of a cluster, as they may cause ABI incompatibilities if the package versions mismatch the driver versions. Fromager will not use them if the entries are left as `~`, which means "null". 

Users are supposed to find the packages installed by their system administrators or contact them if certain packages are not available.

## Initialization of Fromager Toolchain

To make Fromager as distribution-independent as possible, it creates a mini toolchain which contains a GCC compiler, GNU build system, cmake set, and other essential tools. This toolchain is isolated from the system environment and can be easily managed within the Fromager environment.

This process takes however quite some time and compute power, so it is recommended to run it on a dedicated compute node.

You can initialize the toolchain by running the following command:

```bash
fgr initialize
```

or

```bash
fgr init
```

## Usage

You can find all available commands and their documentation by running

```bash
fgr --help
```