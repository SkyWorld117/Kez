#!/usr/bin/env bash

# Execute the shell plan emitted by the C++ command-line parser. Package metadata is never
# reparsed here: this script owns only process execution, installed-state tracking, and cleanup.

set -Eeuo pipefail

# Ensure KEZ_HOME is set; default to the project root (parent of the scripts directory)
: ${KEZ_HOME:=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}

kez_print_fn() {
    local level=$1
    shift
    if [[ $level == error ]]; then
        printf '[E]: %s\n' "$*" >&2
    elif [[ $level == warning ]]; then
        printf '[W]: %s\n' "$*" >&2
    elif [[ $level == info ]]; then
        printf '[I]: %s\n' "$*"
    elif [[ $level == success ]]; then
        printf '[S]: %s\n' "$*"
    else
        printf '%s\n' "$*"
    fi
}

# KEZ_PRINT=${KEZ_HOME}/bin/kez_print
if [[ -f ${KEZ_HOME}/bin/kez_print ]]; then
    KEZ_PRINT=${KEZ_HOME}/bin/kez_print
else
    KEZ_PRINT=kez_print_fn
fi

if [[ $# -lt 2 || $# -gt 3 ]]; then
    "${KEZ_PRINT}" error "Usage: install.sh <environment> <plan> [--force]"
    exit 2
fi

target_env=$1
plan_file=$2
force=false
if [[ ${3:-} == "--force" ]]; then
    force=true
elif [[ -n ${3:-} ]]; then
    "${KEZ_PRINT}" error "Unknown option: $3"
    exit 2
fi

if [[ ! -f $plan_file ]] || [[ $(head -n 1 -- "$plan_file") != "# kez-install-plan-v1" ]]; then
    "${KEZ_PRINT}" error "Invalid Kez installation plan: $plan_file"
    exit 2
fi

install_jobs=${KEZ_INSTALL_JOBS:-1}
if [[ ! $install_jobs =~ ^[1-9][0-9]*$ ]]; then
    "${KEZ_PRINT}" error "KEZ_INSTALL_JOBS must be a positive integer"
    exit 2
fi

# Application environments store an ordered sequence of package names.  The
# system environment built by init.sh stores a package-to-version map instead.
state_format=${KEZ_INSTALL_STATE_FORMAT:-sequence}
if [[ $state_format != sequence && $state_format != map ]]; then
    "${KEZ_PRINT}" error "KEZ_INSTALL_STATE_FORMAT must be 'sequence' or 'map'"
    exit 2
fi

mkdir -p -- "$target_env/.tmp" "$target_env/logs"
state_file="$target_env/state.yaml"
if [[ ! -f $state_file ]]; then
    printf 'state:\n' > "$state_file"
fi

plan_runtime_dir="$target_env/.tmp/.kez-plan-$$"
rm -rf -- "$plan_runtime_dir"
mkdir -p -- "$plan_runtime_dir"

cleanup_runtime() {
    rm -rf -- "$plan_runtime_dir"
}
trap cleanup_runtime EXIT

declare -a plan_packages=()
declare -a package_status=()
declare -a package_command_counts=()
declare -a package_pids=()
declare -A package_indices=()
current_package_index=-1
running_count=0

sanitize_name() {
    local value=$1
    value=${value//[^A-Za-z0-9_.@+-]/_}
    if [[ -z $value ]]; then
        value=package
    fi
    printf '%s\n' "$value"
}

package_log_file() {
    printf '%s/logs/%s.log\n' "$target_env" "$(sanitize_name "$1")"
}

package_work_dir() {
    printf '%s/.tmp/%s\n' "$target_env" "$(sanitize_name "$1")"
}

package_is_recorded() {
    if [[ $state_format == map ]]; then
        PACKAGE_NAME=$1 yq -e '.state | has(strenv(PACKAGE_NAME))' "$state_file" >/dev/null 2>&1
    else
        grep -Fqx -- "  - $1" "$state_file"
    fi
}

mark_package_installed() {
    local package=$1
    local index=$2

    if package_is_recorded "$package"; then
        return 0
    fi

    if [[ $state_format == map ]]; then
        local version_file="$plan_runtime_dir/$index/version"
        local version
        if [[ ! -s $version_file ]]; then
            "${KEZ_PRINT}" error "Package $package did not report its installed version"
            return 2
        fi
        version=$(<"$version_file")
        PACKAGE_NAME=$package PACKAGE_VERSION=$version \
            yq -i '.state[strenv(PACKAGE_NAME)] = strenv(PACKAGE_VERSION)' "$state_file"
    else
        printf '  - %s\n' "$package" >> "$state_file"
    fi
}

cleanup_package_work_dir() {
    rm -rf -- "$(package_work_dir "$1")"
}

kez_plan_begin() {
    if (( current_package_index >= 0 )); then
        "${KEZ_PRINT}" error "Invalid installation plan: nested package"
        return 2
    fi

    local package=$1
    if [[ -n ${package_indices[$package]+set} ]]; then
        "${KEZ_PRINT}" error "Invalid installation plan: duplicate package '$package'"
        return 2
    fi

    local index=${#plan_packages[@]}
    plan_packages+=("$package")
    package_indices["$package"]=$index
    package_status[$index]=pending
    package_command_counts[$index]=0
    package_pids[$index]=
    mkdir -p -- "$plan_runtime_dir/$index"
    : > "$plan_runtime_dir/$index/dependencies"
    current_package_index=$index
}

kez_plan_depends() {
    if (( current_package_index < 0 )); then
        "${KEZ_PRINT}" error "Invalid installation plan: dependency outside a package"
        return 2
    fi
    printf '%s\n' "$1" >> "$plan_runtime_dir/$current_package_index/dependencies"
}

kez_plan_command() {
    if (( current_package_index < 0 )); then
        "${KEZ_PRINT}" error "Invalid installation plan: command outside a package"
        return 2
    fi

    local count=${package_command_counts[$current_package_index]}
    printf '%s' "$1" > "$plan_runtime_dir/$current_package_index/command-$count"
    package_command_counts[$current_package_index]=$((count + 1))
}

kez_plan_end() {
    if (( current_package_index < 0 )); then
        "${KEZ_PRINT}" error "Invalid installation plan: package end without package"
        return 2
    fi
    current_package_index=-1
}

# The plan contains only shell-quoted calls to the functions above. Commands from package metadata
# are evaluated exclusively by run_package_body after dependency scheduling.
source "$plan_file"

if (( current_package_index >= 0 )); then
    "${KEZ_PRINT}" error "Invalid installation plan: package was not closed"
    exit 2
fi

for index in "${!plan_packages[@]}"; do
    package=${plan_packages[$index]}
    if [[ $force == false ]] && package_is_recorded "$package"; then
        "${KEZ_PRINT}" warning "Package $package is already installed; skipping."
        package_status[$index]=skipped
    fi
done

execute_package_command() {
    local command=$1
    local log_file=$2
    local status

    {
        printf '\n[%s] command\n' "$(date -Is)"
        printf '$ %s\n' "$command"
    } >> "$log_file"

    eval "$command" >> "$log_file" 2>&1
    status=$?
    if (( status == 0 )); then
        printf '[%s] completed\n' "$(date -Is)" >> "$log_file"
        return 0
    fi

    printf '[%s] failed with exit status %s\n' "$(date -Is)" "$status" >> "$log_file"
    return "$status"
}

run_package_body() {
    local index=$1
    local package=${plan_packages[$index]}
    local log_file
    local work_dir
    local command
    local command_index
    local status

    log_file=$(package_log_file "$package")
    work_dir=$(package_work_dir "$package")
    cleanup_package_work_dir "$package" || return 1
    mkdir -p -- "$work_dir" || return 1
    : > "$log_file" || return 1
    {
        printf 'Kez install log\n'
        printf 'Package: %s\n' "$package"
        printf 'Environment: %s\n' "$target_env"
        printf 'Started: %s\n' "$(date -Is)"
    } >> "$log_file"

    cd "$work_dir" || return 1
    if [[ $state_format == map ]]; then
        export KEZ_PACKAGE_VERSION_FILE="$plan_runtime_dir/$index/version"
        rm -f -- "$KEZ_PACKAGE_VERSION_FILE"
    else
        unset KEZ_PACKAGE_VERSION_FILE
    fi
    for ((command_index = 0; command_index < package_command_counts[$index]; ++command_index)); do
        command=$(<"$plan_runtime_dir/$index/command-$command_index")
        execute_package_command "$command" "$log_file"
        status=$?
        if (( status != 0 )); then
            return "$status"
        fi
    done
    printf 'Finished: %s\n' "$(date -Is)" >> "$log_file"
    return 0
}

package_ready() {
    local index=$1
    local dependency
    local dependency_index
    local dependency_status

    while IFS= read -r dependency; do
        if [[ -z $dependency || -z ${package_indices[$dependency]+set} ]]; then
            continue
        fi
        dependency_index=${package_indices[$dependency]}
        dependency_status=${package_status[$dependency_index]}
        if [[ $dependency_status != done && $dependency_status != skipped ]]; then
            return 1
        fi
    done < "$plan_runtime_dir/$index/dependencies"
    return 0
}

next_ready_index=-1
find_next_ready_package() {
    local index
    next_ready_index=-1
    for index in "${!plan_packages[@]}"; do
        if [[ ${package_status[$index]} == pending ]] && package_ready "$index"; then
            next_ready_index=$index
            return 0
        fi
    done
    return 0
}

start_package() {
    local index=$1
    local package=${plan_packages[$index]}
    local log_file
    log_file=$(package_log_file "$package")

    "${KEZ_PRINT}" info "Processing package: $package (log: $log_file)"
    package_status[$index]=running
    (
        trap - EXIT
        set +e
        run_package_body "$index"
    ) &
    package_pids[$index]=$!
    (( running_count += 1 ))
}

has_pending_packages() {
    local index
    for index in "${!plan_packages[@]}"; do
        if [[ ${package_status[$index]} == pending ]]; then
            return 0
        fi
    done
    return 1
}

failure_status=0
while true; do
    # Start as many ready packages as the job limit and failure status allow
    while (( failure_status == 0 && running_count < install_jobs )); do
        find_next_ready_package
        if (( next_ready_index < 0 )); then
            break
        fi
        start_package "$next_ready_index"
    done

    # If nothing is running, all pending packages have unmet dependencies
    # or we're out of work — exit the scheduling loop.
    if (( running_count == 0 )); then
        break
    fi

    # Wait for the earliest-started running package (lowest plan index
    # with a non-empty PID).  This guarantees deterministic processing
    # order: packages are completed in the same order they were started.
    completed_index=-1
    completed_status=0
    for (( index = 0; index < ${#plan_packages[@]}; index++ )); do
        pid=${package_pids[$index]:-}
        if [[ -z $pid ]]; then
            # Already processed; skip to the next.
            continue
        fi
        wait "$pid" 2>/dev/null && completed_status=0 || completed_status=$?
        package_pids[$index]=
        completed_index=$index
        break
    done

    if (( completed_index < 0 )); then
        # No tracked child exited — this shouldn't happen, but protect
        # against an infinite loop.
        "${KEZ_PRINT}" error "Installation error: lost track of a running package"
        exit 2
    fi

    package=${plan_packages[$completed_index]}
    log_file=$(package_log_file "$package")
    running_count=$(( running_count - 1 ))

    if (( completed_status == 0 )); then
        package_status[$completed_index]=done
        mark_package_installed "$package" "$completed_index"
        cleanup_package_work_dir "$package"
        "${KEZ_PRINT}" success "Completed package: $package"
    else
        package_status[$completed_index]=failed
        # Do not clean up the work directory on failure, so the user can
        # inspect it for debugging.
        "${KEZ_PRINT}" error "Package $package failed. Read log file: $log_file"
        if (( failure_status == 0 )); then
            failure_status=$completed_status
        fi
    fi
done

if (( failure_status != 0 )); then
    exit "$failure_status"
fi

if has_pending_packages; then
    "${KEZ_PRINT}" error "Invalid installation plan: dependency cycle or missing dependency prevented progress"
    exit 2
fi

if [[ $state_format == sequence ]] && command -v yq >/dev/null 2>&1 && \
    [[ -n ${KEZ_HOME:-} && -n ${KEZ_WORKDIR:-} ]]; then
    modulefiles_path=$(yq -r '.paths.modulefiles' "$KEZ_HOME/manifest.yaml")
    if [[ $modulefiles_path == /* ]]; then
        modulefiles_dir=$modulefiles_path
    else
        modulefiles_dir="$KEZ_WORKDIR/$modulefiles_path"
    fi
    mkdir -p -- "$modulefiles_dir"
    modulefile="$modulefiles_dir/$(basename -- "$target_env")"
    if [[ ! -f $modulefile ]]; then
        "${KEZ_HOME}/scripts/gen_modulefile.sh" "$target_env" > "$modulefile"
        "${KEZ_PRINT}" success "Created module file: $modulefile"
    fi
fi
