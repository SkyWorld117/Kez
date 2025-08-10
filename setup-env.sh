#!/usr/bin/env bash

set -Euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Default work directory for Fromager is ~/.fromager
if [ -z "${FROMAGER_WORKDIR:-}" ]; then
    export FROMAGER_WORKDIR="${HOME}/.fromager"
fi

export FROMAGER_ENV="${FROMAGER_WORKDIR}/cellars"
export FROMAGER_STATE="${FROMAGER_WORKDIR}/state"

export FROMAGER_HOME="${SCRIPT_DIR}"
export FROMAGER_SRC="${FROMAGER_HOME}/src"
export FROMAGER_DB="${FROMAGER_HOME}/database"

# Fromager binaries, included to PATH only for now
# It may interfere with bash auto-completion
if [[ ":$PATH:" != *":${FROMAGER_HOME}/bin:"* ]]; then
    export PATH="${FROMAGER_HOME}/bin:${PATH}"
fi

# Check if `system/bin` and `toolkits/bin` are already in PATH
if [[ ":$PATH:" != *":${FROMAGER_ENV}/system/bin:"* ]]; then
    export PATH="${FROMAGER_ENV}/system/bin:${PATH}"
fi
if [[ ":$PATH:" != *":${FROMAGER_ENV}/toolkits/bin:"* ]]; then
    export PATH="${FROMAGER_ENV}/toolkits/bin:${PATH}"
fi

# Load main script
source "${SCRIPT_DIR}/main.sh"

# Prepare yq for YAML processing
if [ ! -f "${FROMAGER_ENV}/system/bin/yq" ]; then
    mkdir -p "${FROMAGER_ENV}/system/bin"
    if [ "$(uname -m)" = "x86_64" ]; then
        wget --quiet --show-progress --output-document="${FROMAGER_ENV}/system/bin/yq" https://github.com/mikefarah/yq/releases/latest/download/yq_linux_amd64
    elif [ "$(uname -m)" = "aarch64" ] || [ "$(uname -m)" = "arm64" ]; then
        wget --quiet --show-progress --output-document="${FROMAGER_ENV}/system/bin/yq" https://github.com/mikefarah/yq/releases/latest/download/yq_linux_arm64
    else
        echo "Unsupported architecture: $(uname -m)"
        return 1
    fi
    chmod +x "${FROMAGER_ENV}/system/bin/yq"
fi

# Ensure the configuration file exists
if [ ! -f "${FROMAGER_WORKDIR}/config.yaml" ]; then
    mkdir -p "${FROMAGER_WORKDIR}"
    cp "${SCRIPT_DIR}/config.yaml" "${FROMAGER_WORKDIR}/config.yaml"
fi

# Load configuration settings
export FROMAGER_NPROC=$(yq -r '.fromager.n_proc_for_build' "${FROMAGER_WORKDIR}/config.yaml")

set +Euo pipefail