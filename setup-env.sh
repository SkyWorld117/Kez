#!/usr/bin/env bash

set -Euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ -z "${KEZ_WORKDIR:-}" ]; then

    echo "[E]: KEZ_WORKDIR is not set. Please set it to the desired work directory for Kez (e.g., export KEZ_WORKDIR=~/.kez)."

elif [ ! -f "${KEZ_WORKDIR}/config.yaml" ]; then

    mkdir -p "${KEZ_WORKDIR}"
    cp "${SCRIPT_DIR}/configs/config.yaml" "${KEZ_WORKDIR}/config.yaml"
    echo "[I]: \`config.yaml\` was not found in ${KEZ_WORKDIR}. A default config file has been copied from the script directory. Please review and customize it as needed."
    echo "[I]: After setting up the configuration, please re-run this script to initialize the environment variables and load the necessary functions."
    echo "[I]: You can find more example configurations in the ${SCRIPT_DIR}/configs directory."

else
    export KEZ_HOME="${SCRIPT_DIR}"

    # Check if yq is available, if not, it will be installed in the WORKDIR/bin directory
    if [[ ":$PATH:" != *":${KEZ_WORKDIR}/bin:"* ]]; then
        export PATH="${KEZ_WORKDIR}/bin:${PATH}"
    fi

    if [ "$(uname -m)" = "x86_64" ]; then
        export KEZ_ARCH="x86_64"
    elif [ "$(uname -m)" = "aarch64" ] || [ "$(uname -m)" = "arm64" ]; then
        export KEZ_ARCH="arm64"
    else
        echo "Unsupported architecture: $(uname -m)"
        return 1
    fi

    if ! command -v yq &> /dev/null; then
        echo "[I]: yq is not installed. It will be installed in ${KEZ_WORKDIR}/bin."
        mkdir -p "${KEZ_WORKDIR}/bin"

        YQ_VERSION=$(grep -E '^[[:space:]]*yq:' "${KEZ_HOME}/manifest.yaml" | sed -E 's/^[[:space:]]*yq:[[:space:]]*//')
        YQ_ARCH_SUFFIX=""
        if [ "${KEZ_ARCH}" = "x86_64" ]; then
            YQ_ARCH_SUFFIX="_amd64"
        elif [ "${KEZ_ARCH}" = "arm64" ]; then
            YQ_ARCH_SUFFIX="_arm64"
        fi

        if [ "${YQ_VERSION}" = "latest" ]; then
            wget --quiet --show-progress --output-document="${KEZ_WORKDIR}/bin/yq" https://github.com/mikefarah/yq/releases/latest/download/yq_linux"${YQ_ARCH_SUFFIX}"
        else
            wget --quiet --show-progress --output-document="${KEZ_WORKDIR}/bin/yq" https://github.com/mikefarah/yq/releases/download/v"${YQ_VERSION}"/yq_linux"${YQ_ARCH_SUFFIX}"
        fi

        unset YQ_VERSION YQ_ARCH_SUFFIX

        chmod +x "${KEZ_WORKDIR}/bin/yq"
    fi

    export KEZ_DB="${KEZ_HOME}/database"
    export KEZ_ENV="${KEZ_WORKDIR}/$(yq -r '.paths.environment' "${KEZ_HOME}/manifest.yaml")"

    SYSTEM_PATH="${KEZ_WORKDIR}/$(yq -r '.paths.system' "${KEZ_HOME}/manifest.yaml")"
    UTILITIES_PATH="${KEZ_WORKDIR}/$(yq -r '.paths.utilities' "${KEZ_HOME}/manifest.yaml")"

    # Add the system bin directory to PATH (single, no subdirectories)
    if [[ -d "${SYSTEM_PATH}/bin" && ":$PATH:" != *":${SYSTEM_PATH}/bin:"* ]]; then
        export PATH="${SYSTEM_PATH}/bin:${PATH}"
    fi

    # Utilities may be split into per-package subdirectories; add each bin/.
    for util_dir in "${UTILITIES_PATH}"/*/; do
        [[ -d "${util_dir}/bin" ]] || continue
        if [[ ":$PATH:" != *":${util_dir}/bin:"* ]]; then
            export PATH="${util_dir}/bin:${PATH}"
        fi
    done

    unset SYSTEM_PATH UTILITIES_PATH

    # Load main script
    source "${KEZ_HOME}/main.sh"
    source "${KEZ_HOME}/completion.bash"

    # Load configuration settings
    export KEZ_NPROC=$(yq -r '.settings.n_proc_for_build' "${KEZ_WORKDIR}/config.yaml")

    # Add the work directory to MODULEPATH for module system integration
    MODULEFILES=${KEZ_WORKDIR}/$(yq -r '.paths.modulefiles' "${KEZ_HOME}/manifest.yaml")
    mkdir -p "${MODULEFILES}"
    if [[ ":${MODULEPATH:-}:" != *":${MODULEFILES}:"* ]]; then
        export MODULEPATH="${MODULEFILES}:${MODULEPATH:-}"
    fi

    unset MODULEFILES

fi

unset SCRIPT_DIR

set +euo pipefail
