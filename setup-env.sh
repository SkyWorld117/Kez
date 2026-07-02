#!/usr/bin/env bash

set -Euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Fetch project name from `manifest.yaml` without any external dependencies
PROJECT=$(sed -n '/project:/ {n; s/.*name:[[:space:]]*//p; q;}' ${SCRIPT_DIR}/manifest.yaml)
PREFIX=${PROJECT^^}  # Convert to uppercase
BINARY=${PROJECT,,}  # Convert to lowercase
ABBREV=$(grep -E '^[[:space:]]*abbrev:' ${SCRIPT_DIR}/manifest.yaml | sed -E 's/^[[:space:]]*abbrev:[[:space:]]*//')

# Define environment variable names based on the project name
_WORKDIR="${PREFIX}_WORKDIR"
_HOME="${PREFIX}_HOME"
_DB="${PREFIX}_DB"
_ENV="${PREFIX}_ENV"
_ARCH="${PREFIX}_ARCH"
_NPROC="${PREFIX}_NPROC"

if [ -z "${!_WORKDIR:-}" ]; then

    echo "[E]: ${_WORKDIR} environment variable is not set. Please set it to the desired work directory for ${PREFIX} (e.g., export ${_WORKDIR}=~/.${PREFIX,,})."

elif [ ! -f "${!_WORKDIR}/config.yaml" ]; then

    mkdir -p "${!_WORKDIR}"
    cp "${SCRIPT_DIR}/configs/config.yaml" "${!_WORKDIR}/config.yaml"
    echo "[I]: \`config.yaml\` was not found in ${!_WORKDIR}. A default config file has been copied from the script directory. Please review and customize it as needed."
    echo "[I]: After setting up the configuration, please re-run this script to initialize the environment variables and load the necessary functions."
    echo "[I]: You can find more example configurations in the ${SCRIPT_DIR}/configs directory."

else

    # Check if yq is available, if not, it will be installed in the WORKDIR/bin directory
    if [[ ":$PATH:" != *":${!_WORKDIR}/bin:"* ]]; then
        export PATH="${!_WORKDIR}/bin:${PATH}"
    fi

    if [ "$(uname -m)" = "x86_64" ]; then
        export "${_ARCH}"="x86_64"
    elif [ "$(uname -m)" = "aarch64" ] || [ "$(uname -m)" = "arm64" ]; then
        export "${_ARCH}"="arm64"
    else
        echo "Unsupported architecture: $(uname -m)"
        return 1
    fi

    if ! command -v yq &> /dev/null; then
        echo "[I]: yq is not installed. It will be installed in ${!_WORKDIR}/bin."
        mkdir -p "${!_WORKDIR}/bin"
        
        YQ_VERSION=$(grep -E '^[[:space:]]*yq:' "${!_HOME}/manifest.yaml" | sed -E 's/^[[:space:]]*yq:[[:space:]]*//')
        YQ_ARCH_SUFFIX=""
        if [ "${!_ARCH}" = "x86_64" ]; then
            YQ_ARCH_SUFFIX="_amd64"
        elif [ "${!_ARCH}" = "arm64" ]; then
            YQ_ARCH_SUFFIX="_arm64"
        fi

        if [ "${YQ_VERSION}" = "latest" ]; then
            wget --quiet --show-progress --output-document="${!_WORKDIR}/bin/yq" https://github.com/mikefarah/yq/releases/latest/download/yq_linux"${YQ_ARCH_SUFFIX}"
        else
            wget --quiet --show-progress --output-document="${!_WORKDIR}/bin/yq" https://github.com/mikefarah/yq/releases/download/v"${YQ_VERSION}"/yq_linux"${YQ_ARCH_SUFFIX}"
        fi
        
        unset YQ_VERSION YQ_ARCH_SUFFIX

        chmod +x "${!_WORKDIR}/bin/yq"
    fi

    export "${_HOME}"="${SCRIPT_DIR}"
    export "${_DB}"="${!_HOME}/database"
    export "${_ENV}"="${!_HOME}/$(yq -r '.paths.environment' ${!_HOME}/manifest.yaml)"

    SYSTEM_BIN="${!_WORKDIR}/$(yq -r '.paths.system' "${!_HOME}/manifest.yaml")/bin"
    UTILITIES_BIN="${!_WORKDIR}/$(yq -r '.paths.utilities' "${!_HOME}/manifest.yaml")/bin"

    if [[ ":$PATH:" != *":${SYSTEM_BIN}:"* ]]; then
        export PATH="${SYSTEM_BIN}:${PATH}"
    fi
    if [[ ":$PATH:" != *":${UTILITIES_BIN}:"* ]]; then
        export PATH="${UTILITIES_BIN}:${PATH}"
    fi

    unset SYSTEM_BIN UTILITIES_BIN

    # Load main script
    source "${!_HOME}/main.sh"
    source "${!_HOME}/completion.bash"

    # Load configuration settings
    export "${_NPROC}"=$(yq -r '.settings.n_proc_for_build' "${!_WORKDIR}/config.yaml")

    # Add the work directory to MODULEPATH for module system integration
    MODULEFILES=${!_WORKDIR}/$(yq -r '.paths.modulefiles' "${!_HOME}/manifest.yaml")
    mkdir -p "${MODULEFILES}"
    if [[ ":${MODULEPATH:-}:" != *":${MODULEFILES}:"* ]]; then
        export MODULEPATH="${MODULEFILES}:${MODULEPATH:-}"
    fi

    unset MODULEFILES

fi

unset SCRIPT_DIR 
unset PROJECT PREFIX BINARY ABBREV
unset _WORKDIR _HOME _DB _ENV _ARCH _NPROC

set +euo pipefail
