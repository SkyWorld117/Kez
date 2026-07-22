#!/usr/bin/env bash

# Reconcile one shared Python virtual environment with the exact requirements
# represented by Python-toolchain package nodes. Requirements are always
# installed together into a fresh venv, so pip resolves one deterministic
# transaction and never races with another package worker.

set -Eeuo pipefail

: "${KEZ_HOME:=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
source "${KEZ_HOME}/scripts/colored_io.sh"

stage_dir=
venv_backup=
state_backup=
target_env=

cleanup() {
    local status=$?
    if (( status != 0 )) && [[ -n $target_env ]]; then
        if [[ -n $venv_backup && -d $venv_backup ]]; then
            rm -rf -- "$target_env/.venv"
            mv -- "$venv_backup" "$target_env/.venv"
        fi
        if [[ -n $state_backup && -d $state_backup ]]; then
            rm -rf -- "$target_env/.kez-python"
            mv -- "$state_backup" "$target_env/.kez-python"
        fi
    fi
    if [[ -n $stage_dir && -d $stage_dir ]]; then
        rm -rf -- "$stage_dir"
    fi
    if [[ -n $venv_backup && -d $venv_backup ]]; then
        rm -rf -- "$venv_backup"
    fi
    if [[ -n $state_backup && -d $state_backup ]]; then
        rm -rf -- "$state_backup"
    fi
    return "$status"
}
trap cleanup EXIT

usage() {
    error "Usage: python_env.sh <sync-plan ENV PLAN_RUNTIME|remove-package ENV PACKAGE|register-package ENV PACKAGE>"
    exit 2
}

validate_package_name() {
    if [[ ! $1 =~ ^[A-Za-z0-9][A-Za-z0-9_.+-]*$ ]]; then
        error "Invalid package name in Python environment state: $1"
        exit 2
    fi
}

prepare_stage() {
    target_env=$1
    if [[ -z $target_env || $target_env == / ]]; then
        error "Invalid target environment for Python environment synchronization"
        exit 2
    fi
    mkdir -p -- "$target_env/.tmp"
    stage_dir=$(mktemp -d "$target_env/.tmp/.kez-python-state.XXXXXX")
    mkdir -p -- "$stage_dir/packages"
    if [[ -d $target_env/.kez-python/packages ]]; then
        cp -a -- "$target_env/.kez-python/packages/." "$stage_dir/packages/"
    fi
}

apply_plan_declarations() {
    local plan_runtime=$1
    local package_dir package requirement_file
    if [[ ! -d $plan_runtime ]]; then
        error "Python environment plan runtime does not exist: $plan_runtime"
        exit 2
    fi

    for package_dir in "$plan_runtime"/*; do
        [[ -f $package_dir/package-name ]] || continue
        IFS= read -r package < "$package_dir/package-name"
        if [[ $package == .kez-python-environment ]]; then
            continue
        fi
        validate_package_name "$package"
        rm -f -- "$stage_dir/packages/$package.requirements"

        # Installing Python itself creates an environment even if no package
        # has declared a PyPI requirement yet.
        if [[ -f $package_dir/provides-python-environment ||
            -f $package_dir/uses-python-environment ]]; then
            requirement_file="$package_dir/python-requirements"
            if [[ -f $requirement_file ]]; then
                cp -- "$requirement_file" "$stage_dir/packages/$package.requirements"
            else
                : > "$stage_dir/packages/$package.requirements"
            fi
        fi
    done
}

remove_package_declaration() {
    local package=$1
    validate_package_name "$package"
    rm -f -- "$stage_dir/packages/$package.requirements"
}

virtual_environment_site_packages() {
    local -a candidates=()
    shopt -s nullglob
    candidates=("$target_env/.venv/lib/"python*/site-packages)
    shopt -u nullglob
    if (( ${#candidates[@]} != 0 )); then
        printf '%s\n' "${candidates[0]}"
    fi
}

register_package_paths() {
    local package=$1
    local site_packages path_file path
    local -a paths=()
    validate_package_name "$package"
    [[ $package != python ]] || return 0
    site_packages=$(virtual_environment_site_packages)
    [[ -n $site_packages ]] || return 0

    path_file="$site_packages/kez-$package.pth"
    shopt -s nullglob
    paths=("$target_env/$package/lib/"python*/site-packages
           "$target_env/$package/lib64/"python*/site-packages)
    shopt -u nullglob
    rm -f -- "$path_file"
    for path in "${paths[@]}"; do
        [[ -d $path ]] || continue
        printf '%s\n' "$path" >> "$path_file"
    done
}

refresh_package_paths() {
    local site_packages package_dir package path_file
    local -a managed_paths=()
    site_packages=$(virtual_environment_site_packages)
    [[ -n $site_packages ]] || return 0

    shopt -s nullglob
    managed_paths=("$site_packages/"kez-*.pth)
    shopt -u nullglob
    for path_file in "${managed_paths[@]}"; do
        rm -f -- "$path_file"
    done
    for package_dir in "$target_env"/*; do
        [[ -d $package_dir ]] || continue
        package=$(basename -- "$package_dir")
        [[ $package =~ ^[A-Za-z0-9][A-Za-z0-9_.+-]*$ ]] || continue
        register_package_paths "$package"
    done
}

write_combined_requirements() {
    local file
    local -a package_files=()
    shopt -s nullglob
    package_files=("$stage_dir/packages/"*.requirements)
    shopt -u nullglob
    if (( ${#package_files[@]} == 0 )); then
        return 1
    fi

    : > "$stage_dir/requirements.txt"
    for file in "${package_files[@]}"; do
        while IFS= read -r requirement || [[ -n $requirement ]]; do
            [[ -n $requirement ]] || continue
            printf '%s\n' "$requirement" >> "$stage_dir/requirements.txt"
        done < "$file"
    done
    LC_ALL=C sort -u -o "$stage_dir/requirements.txt" "$stage_dir/requirements.txt"
    return 0
}

python_signature() {
    local python=$1
    env -u PYTHONHOME -u PYTHONPATH PYTHONNOUSERSITE=1 "$python" -c \
        'import sys; print(sys.version.split()[0] + " " + (sys.implementation.cache_tag or ""))'
}

replace_state() {
    if [[ -d $target_env/.kez-python ]]; then
        state_backup="$target_env/.tmp/.kez-python-backup.$$"
        rm -rf -- "$state_backup"
        mv -- "$target_env/.kez-python" "$state_backup"
    fi
    mv -- "$stage_dir" "$target_env/.kez-python"
    stage_dir=
    if [[ -n $state_backup && -d $state_backup ]]; then
        rm -rf -- "$state_backup"
    fi
    state_backup=
}

remove_python_environment() {
    rm -rf -- "$target_env/.venv" "$target_env/.kez-python"
    info "Removed unused Python virtual environment from $target_env"
}

rebuild_virtual_environment() {
    local base_python=$1
    local pip_cache="$target_env/.tmp/pip-cache"
    local requirement
    local -a requirements=()

    if [[ -d $target_env/.venv ]]; then
        venv_backup="$target_env/.tmp/.venv-backup.$$"
        rm -rf -- "$venv_backup"
        mv -- "$target_env/.venv" "$venv_backup"
    fi

    env -u PYTHONHOME -u PYTHONPATH PYTHONNOUSERSITE=1 \
        "$base_python" -m venv "$target_env/.venv"
    while IFS= read -r requirement || [[ -n $requirement ]]; do
        [[ -n $requirement ]] || continue
        requirements+=("$requirement")
    done < "$stage_dir/requirements.txt"
    if (( ${#requirements[@]} != 0 )); then
        mkdir -p -- "$pip_cache"
        env -u PYTHONHOME -u PYTHONPATH PYTHONNOUSERSITE=1 PIP_CACHE_DIR="$pip_cache" \
            "$target_env/.venv/bin/python" -m pip install --disable-pip-version-check \
            "${requirements[@]}"
    fi
    env -u PYTHONHOME -u PYTHONPATH PYTHONNOUSERSITE=1 \
        "$target_env/.venv/bin/python" -m pip freeze --all |
        LC_ALL=C sort > "$stage_dir/resolved.txt"

}

synchronize() {
    if ! write_combined_requirements; then
        remove_python_environment
        return
    fi

    local base_python="$target_env/python/bin/python3"
    if [[ ! -x $base_python ]]; then
        error "Python interpreter is missing from the Kez environment: $base_python"
        exit 1
    fi

    local real_python signature rebuild=true
    real_python=$(readlink -f -- "$base_python")
    signature="$real_python $(python_signature "$base_python")"
    printf '%s\n' "$signature" > "$stage_dir/base.txt"

    if [[ -x $target_env/.venv/bin/python && -f $target_env/.kez-python/base.txt &&
        -f $target_env/.kez-python/requirements.txt ]] &&
        cmp -s -- "$stage_dir/base.txt" "$target_env/.kez-python/base.txt" &&
        cmp -s -- "$stage_dir/requirements.txt" "$target_env/.kez-python/requirements.txt"; then
        rebuild=false
    fi

    if [[ $rebuild == true ]]; then
        rebuild_virtual_environment "$base_python"
        success "Synchronized Python virtual environment: $target_env/.venv"
    elif [[ -f $target_env/.kez-python/resolved.txt ]]; then
        cp -- "$target_env/.kez-python/resolved.txt" "$stage_dir/resolved.txt"
    else
        : > "$stage_dir/resolved.txt"
    fi
    refresh_package_paths
    replace_state
    if [[ -n $venv_backup && -d $venv_backup ]]; then
        rm -rf -- "$venv_backup"
    fi
    venv_backup=
}

if (( $# < 1 )); then
    usage
fi

mode=$1
shift
case $mode in
    sync-plan)
        (( $# == 2 )) || usage
        prepare_stage "$1"
        apply_plan_declarations "$2"
        synchronize
        ;;
    remove-package)
        (( $# == 2 )) || usage
        prepare_stage "$1"
        remove_package_declaration "$2"
        synchronize
        ;;
    register-package)
        (( $# == 2 )) || usage
        target_env=$1
        if [[ -d $target_env/.venv ]]; then
            register_package_paths "$2"
        fi
        ;;
    *) usage ;;
esac
