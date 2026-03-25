#!/usr/bin/env bash
#SBATCH --job-name=fromager-test-01
#SBATCH --output=fromager-test-01.out
#SBATCH --error=fromager-test-01.err
#SBATCH --nodelist=piora1,piora2
#SBATCH --exclusive

# Test script for Fromager - Basic Functionality Test
# This script is inevitably cluster dependent, but it should be able to run on our internal clusters Sbrinz and Piora without modification.
# Test includes:
# 1. Check if Fromager can be initialized correctly
# 2. Check if we can build a compiler other than the one in the system cellar
# 3. Check if we can build an MPI implementation
# 4. Check if we can build OSU Micro-Benchmarks
# 5. Check if OSU Micro-Benchmarks can be run successfully

set -Eeuo pipefail

# Check if there is already an existing Fromager environment and if so, fetch the config.yaml from it.
STANDARD_CONFIG="${FROMAGER_HOME}/configs/${HOSTNAME%?}.yaml"
if [ -n "${FROMAGER_WORKDIR:-}" ] && [ -f "${FROMAGER_WORKDIR}/config.yaml" ]; then
    STANDARD_CONFIG="${FROMAGER_WORKDIR}/config.yaml"
fi

# Override the FROMAGER_WORKDIR for testing
# This effectively creates a separate Fromager environment, preventing interference with any existing Fromager setup on the system.
export FROMAGER_WORKDIR="/scratch/${USER}/.fromager-test"
if [ -d "${FROMAGER_WORKDIR}" ]; then
    echo "[I]: Removing existing test Fromager environment at ${FROMAGER_WORKDIR}..."
    rm -rf "${FROMAGER_WORKDIR}"
fi

# Remove environment variables that might interfere with the test
unset FROMAGER_DB FROMAGER_ENV
# Edit PATH to remove any existing Fromager paths
export PATH=$(echo "$PATH" | tr ':' '\n' | grep -v "${FROMAGER_WORKDIR}" | grep -v "${FROMAGER_HOME}" | tr '\n' ':')

# Set up the test Fromager environment
source "${FROMAGER_HOME}/setup-env.sh"
cp "${STANDARD_CONFIG}" "${FROMAGER_WORKDIR}/config.yaml"
source "${FROMAGER_HOME}/setup-env.sh" # Source again to load the new config

# Initialize Fromager
fgr init

# Build a compiler
fgr install gcc

# Build an MPI implementation
fgr install -r "${FROMAGER_HOME}/examples/openmpi.yaml"

# Build OSU Micro-Benchmarks
fgr cellar create osu-ompi
fgr install -r "${FROMAGER_HOME}/examples/osu-openmpi.yaml" osu-ompi

# Run OSU Micro-Benchmarks
module load slurm
srun --nodes=2 --ntasks-per-node=1 --cpus-per-task=1 --mpi=pmix "${FROMAGER_ENV}/osu-ompi/libexec/osu-micro-benchmarks/mpi/pt2pt/persistent/osu_bibw_persistent"

set +Eeuo pipefail
