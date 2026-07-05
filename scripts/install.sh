#!/usr/bin/env bash

# Execute the shell plan emitted by the C++ command-line parser. Package metadata is never
# reparsed here: this script owns only process execution, installed-state tracking, and cleanup.

set -Eeuo pipefail

if [[ $# -lt 2 || $# -gt 3 ]]; then
    echo >&2 "[E]: Usage: install.sh <environment> <plan> [--force]"
    exit 2
fi

target_env=$1
plan_file=$2
force=false
if [[ ${3:-} == "--force" ]]; then
    force=true
elif [[ -n ${3:-} ]]; then
    echo >&2 "[E]: Unknown option: $3"
    exit 2
fi

if [[ ! -f $plan_file ]] || [[ $(head -n 1 -- "$plan_file") != "# kez-install-plan-v1" ]]; then
    echo >&2 "[E]: Invalid Kez installation plan: $plan_file"
    exit 2
fi

mkdir -p -- "$target_env/.tmp"
state_file="$target_env/state.yaml"
if [[ ! -f $state_file ]]; then
    printf 'state:\n' > "$state_file"
fi

current_package=
current_command=
execute_current_package=false

cleanup_source() {
    rm -rf -- "$target_env/.tmp/source" "$target_env/.tmp"/source*
}

report_failure() {
    local status=$?
    if [[ -n $current_command ]]; then
        echo >&2 "[E]: Command failed for $current_package: $current_command"
    elif [[ -n $current_package ]]; then
        echo >&2 "[E]: Installation failed while processing $current_package"
    else
        echo >&2 "[E]: Installation failed"
    fi
    cleanup_source
    exit "$status"
}

trap report_failure ERR

kez_plan_begin() {
    if [[ -n $current_package ]]; then
        echo >&2 "[E]: Invalid installation plan: nested package"
        return 2
    fi

    current_package=$1
    current_command=
    execute_current_package=true
    cleanup_source
    cd "$target_env/.tmp"

    if [[ $force == false ]] && grep -Fqx -- "  - $current_package" "$state_file"; then
        echo "[W]: Package $current_package is already installed; skipping."
        execute_current_package=false
    else
        echo "[I]: Processing package: $current_package"
    fi
}

kez_plan_command() {
    if [[ -z $current_package ]]; then
        echo >&2 "[E]: Invalid installation plan: command outside a package"
        return 2
    fi
    if [[ $execute_current_package == false ]]; then
        return 0
    fi

    current_command=$1
    echo "[I]: Executing: $current_command"
    eval "$current_command"
    current_command=
}

kez_plan_end() {
    if [[ -z $current_package ]]; then
        echo >&2 "[E]: Invalid installation plan: package end without package"
        return 2
    fi

    if [[ $execute_current_package == true ]]; then
        if ! grep -Fqx -- "  - $current_package" "$state_file"; then
            printf '  - %s\n' "$current_package" >> "$state_file"
        fi
        echo "[S]: Completed package: $current_package"
    fi
    cleanup_source
    current_package=
    current_command=
    execute_current_package=false
}

# The plan contains only shell-quoted calls to the three functions above. Commands from package
# metadata are evaluated exclusively inside kez_plan_command.
source "$plan_file"

if [[ -n $current_package ]]; then
    echo >&2 "[E]: Invalid installation plan: package was not closed"
    exit 2
fi

if command -v yq >/dev/null 2>&1 && [[ -n ${KEZ_HOME:-} && -n ${KEZ_WORKDIR:-} ]]; then
    modulefiles_path=$(yq -r '.paths.modulefiles' "$KEZ_HOME/manifest.yaml")
    if [[ $modulefiles_path == /* ]]; then
        modulefiles_dir=$modulefiles_path
    else
        modulefiles_dir="$KEZ_WORKDIR/$modulefiles_path"
    fi
    mkdir -p -- "$modulefiles_dir"
    modulefile="$modulefiles_dir/$(basename -- "$target_env")"
    if [[ ! -f $modulefile ]]; then
        environment_name=$(basename -- "$target_env")
        tcl_environment_name=${environment_name//\\/\\\\}
        tcl_environment_name=${tcl_environment_name//\"/\\\"}
        tcl_target_env=${target_env//\\/\\\\}
        tcl_target_env=${tcl_target_env//\"/\\\"}
        {
            printf '#%%Module1.0\n'
            printf 'module-whatis "Loads the %s Kez environment"\n' "$tcl_environment_name"
            printf 'prepend-path PATH "%s/bin"\n' "$tcl_target_env"
        } > "$modulefile"
        echo "[S]: Created module file: $modulefile"
    fi
fi
