#!/usr/bin/env bash
#SBATCH --job-name=fromager-test-01
#SBATCH --output=fromager-test-01.out
#SBATCH --error=fromager-test-01.err
#SBATCH --nodes=2
#SBATCH --cpus-per-task=32

# Test script for Fromager - Basic Functionality Test
# This script is inevitably cluster dependent, but it should be able to run on our internal clusters Sbrinz and Piora without modification.
# Test includes:
# 1. Check if we can build a compiler other than the one in the system cellar
# 2. Check if we can build an MPI implementation
# 3. Check if we can build OSU Micro-Benchmarks
# 4. Check if OSU Micro-Benchmarks can be run successfully

set -Eeuo pipefail

export FROMAGER_WORKDIR="/scratch/${USER}/.fromager-test"
source "${FROMAGER_HOME}/setup-env.sh" # Source again to load the new config

# Build a compiler
fgr install gcc

# Build an MPI implementation
fgr install -r "${FROMAGER_HOME}/examples/openmpi.yaml"

# Build OSU Micro-Benchmarks
fgr cellar create osu-ompi
fgr install -r "${FROMAGER_HOME}/examples/osu-openmpi.yaml" --cellar osu-ompi

# Run OSU Micro-Benchmarks
module load slurm
srun --nodes=2 --ntasks-per-node=1 --cpus-per-task=1 --mpi=pmix "${FROMAGER_ENV}/osu-ompi/libexec/osu-micro-benchmarks/mpi/pt2pt/persistent/osu_bibw_persistent"

set +Eeuo pipefail
