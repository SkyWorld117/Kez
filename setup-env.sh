#!/usr/bin/env bash

set -Euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Default work directory for Fromager is ~/.fromager
if [ -z "${FROMAGER_WORKDIR:-}" ]; then

    echo "[E]: FROMAGER_WORKDIR environment variable is not set. Please set it to the desired work directory for Fromager (e.g., export FROMAGER_WORKDIR=~/.fromager)."

elif [ ! -f "${FROMAGER_WORKDIR}/config.yaml" ]; then

    mkdir -p "${FROMAGER_WORKDIR}"
    cp "${SCRIPT_DIR}/configs/config.yaml" "${FROMAGER_WORKDIR}/config.yaml"
    echo "[I]: \`config.yaml\` was not found in ${FROMAGER_WORKDIR}. A default config file has been copied from the script directory. Please review and customize it as needed."
    echo "[I]: After setting up the configuration, please re-run this script to initialize the environment variables and load the necessary functions."

else

    # Check if yq is available, if not, it will be installed in the FROMAGER_WORKDIR/bin directory
    if ! command -v yq &> /dev/null; then
        echo "[I]: yq is not installed. It will be installed in ${FROMAGER_WORKDIR}/bin."
        mkdir -p "${FROMAGER_WORKDIR}/bin"
        if [ "$(uname -m)" = "x86_64" ]; then
            wget --quiet --show-progress --output-document="${FROMAGER_WORKDIR}/bin/yq" https://github.com/mikefarah/yq/releases/latest/download/yq_linux_amd64
        elif [ "$(uname -m)" = "aarch64" ] || [ "$(uname -m)" = "arm64" ]; then
            wget --quiet --show-progress --output-document="${FROMAGER_WORKDIR}/bin/yq" https://github.com/mikefarah/yq/releases/latest/download/yq_linux_arm64
        fi
        chmod +x "${FROMAGER_WORKDIR}/bin/yq"

        if [[ ":$PATH:" != *":${FROMAGER_WORKDIR}/bin:"* ]]; then
            export PATH="${FROMAGER_WORKDIR}/bin:${PATH}"
        fi
    fi

    export FROMAGER_HOME="${SCRIPT_DIR}"
    export FROMAGER_DB="${FROMAGER_HOME}/database"

    export FROMAGER_ENV="${FROMAGER_WORKDIR}/$(yq -r '.fromager.paths.cellars' ${FROMAGER_WORKDIR}/config.yaml)"

    FROMAGER_SYSTEM_BIN="${FROMAGER_WORKDIR}/$(yq -r '.fromager.paths.system' "${FROMAGER_WORKDIR}/config.yaml")/bin"
    FROMAGER_UTILITIES_BIN="${FROMAGER_WORKDIR}/$(yq -r '.fromager.paths.utilities' "${FROMAGER_WORKDIR}/config.yaml")/bin"

    if [[ ":$PATH:" != *":${FROMAGER_SYSTEM_BIN}:"* ]]; then
        export PATH="${FROMAGER_SYSTEM_BIN}:${PATH}"
    fi
    if [[ ":$PATH:" != *":${FROMAGER_UTILITIES_BIN}:"* ]]; then
        export PATH="${FROMAGER_UTILITIES_BIN}:${PATH}"
    fi

    unset FROMAGER_SYSTEM_BIN FROMAGER_UTILITIES_BIN

    if [ "$(uname -m)" = "x86_64" ]; then
        export FROMAGER_ARCH="x86_64"
    elif [ "$(uname -m)" = "aarch64" ] || [ "$(uname -m)" = "arm64" ]; then
        export FROMAGER_ARCH="arm64"
    else
        echo "Unsupported architecture: $(uname -m)"
        return 1
    fi

    # Load main script
    source "${SCRIPT_DIR}/main.sh"
    # Load bash completion script if in bash shell
    if [ -n "${BASH_VERSION:-}" ] && [ -f "${SCRIPT_DIR}/fgr-completion.bash" ]; then
        source "${SCRIPT_DIR}/fgr-completion.bash"
    fi

    # Load configuration settings
    export FROMAGER_NPROC=$(yq -r '.fromager.n_proc_for_build' "${FROMAGER_WORKDIR}/config.yaml")

    set +Euo pipefail

fi
