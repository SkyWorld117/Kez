#!/usr/bin/env bash

set -Euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Default work directory for Fromager is ~/.fromager
if [ -z "${FROMAGER_WORKDIR:-}" ]; then

    echo "[E]: FROMAGER_WORKDIR environment variable is not set. Please set it to the desired work directory for Fromager (e.g., export FROMAGER_WORKDIR=~/.fromager)."

else

    export FROMAGER_ENV="${FROMAGER_WORKDIR}/cellars"

    export FROMAGER_HOME="${SCRIPT_DIR}"
    export FROMAGER_DB="${FROMAGER_HOME}/database"

    if [[ ":$PATH:" != *":${FROMAGER_HOME}/bin:"* ]]; then
        export PATH="${FROMAGER_HOME}/bin:${PATH}"
    fi
    if [[ ":$PATH:" != *":${FROMAGER_ENV}/system/bin:"* ]]; then
        export PATH="${FROMAGER_ENV}/system/bin:${PATH}"
    fi
    if [[ ":$PATH:" != *":${FROMAGER_ENV}/utilities/bin:"* ]]; then
        export PATH="${FROMAGER_ENV}/utilities/bin:${PATH}"
    fi

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

    # Prepare yq for YAML processing
    if [ ! -f "${FROMAGER_ENV}/system/bin/yq" ]; then
        mkdir -p "${FROMAGER_ENV}/system/bin"
        if [ "${FROMAGER_ARCH}" = "x86_64" ]; then
            wget --quiet --show-progress --output-document="${FROMAGER_ENV}/system/bin/yq" https://github.com/mikefarah/yq/releases/latest/download/yq_linux_amd64
        elif [ "${FROMAGER_ARCH}" = "arm64" ]; then
            wget --quiet --show-progress --output-document="${FROMAGER_ENV}/system/bin/yq" https://github.com/mikefarah/yq/releases/latest/download/yq_linux_arm64
        fi
        chmod +x "${FROMAGER_ENV}/system/bin/yq"
    fi

    # Ensure the configuration file exists
    if [ ! -f "${FROMAGER_WORKDIR}/config.yaml" ]; then
        mkdir -p "${FROMAGER_WORKDIR}"
        cp "${SCRIPT_DIR}/configs/config.yaml" "${FROMAGER_WORKDIR}/config.yaml"
    fi

    # Load configuration settings
    export FROMAGER_NPROC=$(yq -r '.fromager.n_proc_for_build' "${FROMAGER_WORKDIR}/config.yaml")

    set +Euo pipefail

fi