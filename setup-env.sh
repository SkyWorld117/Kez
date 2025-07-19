#!/usr/bin/env bash

set -Euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Default work directory for Cheese is ~/.cheese
if [ -z "$CHEESE_WORKDIR" ]; then
    export CHEESE_WORKDIR="${HOME}/.cheese"
fi

export CHEESE_ENV="${CHEESE_WORKDIR}/env"
export CHEESE_STATE="${CHEESE_WORKDIR}/state"

export CHEESE_HOME="${SCRIPT_DIR}"
export CHEESE_SRC="${CHEESE_HOME}/src"
export CHEESE_DB="${CHEESE_HOME}/database"
export PATH="${CHEESE_HOME}/bin:${PATH}"

export PATH="${CHEESE_ENV}/system/bin:${CHEESE_ENV}/toolkits/bin:${PATH}"

# Load main script
source "${SCRIPT_DIR}/main.sh"

# Prepare yq for YAML processing
if [ ! -f "${CHEESE_ENV}/system/bin/yq" ]; then
    mkdir -p "${CHEESE_ENV}/system/bin"
    if [ "$(uname -m)" = "x86_64" ]; then
        wget --quiet --show-progress --output-document="${CHEESE_ENV}/system/bin/yq" https://github.com/mikefarah/yq/releases/latest/download/yq_linux_amd64
    elif [ "$(uname -m)" = "aarch64" ] || [ "$(uname -m)" = "arm64" ]; then
        wget --quiet --show-progress --output-document="${CHEESE_ENV}/system/bin/yq" https://github.com/mikefarah/yq/releases/latest/download/yq_linux_arm64
    else
        echo "Unsupported architecture: $(uname -m)"
        return 1
    fi
    chmod +x "${CHEESE_ENV}/system/bin/yq"
fi

# Ensure the configuration file exists
if [ ! -f "${CHEESE_WORKDIR}/config.yaml" ]; then
    mkdir -p "${CHEESE_WORKDIR}"
    cp "${SCRIPT_DIR}/config.yaml" "${CHEESE_WORKDIR}/config.yaml"
fi

# Load configuration settings
export CHEESE_NPROC=$(yq -r '.cheese.n_proc_for_build' "${CHEESE_WORKDIR}/config.yaml")

set +Euo pipefail