#!/usr/bin/env bash
#SBATCH --job-name=fromager-test-00
#SBATCH --output=fromager-test-00.out
#SBATCH --error=fromager-test-00.err
#SBATCH --nodelist=piora1
#SBATCH --cpus-per-task=32

# Test script for Fromager - Initialization Test
# This script is inevitably cluster dependent, but it should be able to run on our internal clusters Sbrinz and Piora without modification.

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

set +Eeuo pipefail
