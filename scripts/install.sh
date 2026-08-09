#!/usr/bin/env bash

# Execute the shell plan emitted by the C++ command-line parser. Package metadata is never
# reparsed here: this script owns only process execution, installed-state tracking, and cleanup.

set -Eeuo pipefail

SPINNER_INTERVAL_MS=40
ANIMATION_INTERVAL=0.02
PYTHON_ENVIRONMENT_PACKAGE=.kez-python-environment

# Ensure KEZ_HOME is set; default to the project root (parent of the scripts directory)
: ${KEZ_HOME:=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}
source "${KEZ_HOME}/scripts/colored_io.sh"

if [[ $# -lt 2 || $# -gt 3 ]]; then
    error "Usage: install.sh <environment> <plan> [--force]"
    exit 2
fi

target_env=$1
plan_file=$2
force=false
if [[ ${3:-} == "--force" ]]; then
    force=true
elif [[ -n ${3:-} ]]; then
    error "Unknown option: $3"
    exit 2
fi

if [[ ! -f $plan_file ]] || [[ $(head -n 1 -- "$plan_file") != "# kez-install-plan-v1" ]]; then
    error "Invalid Kez installation plan: $plan_file"
    exit 2
fi

if [[ ! ${KEZ_NJOBS:-} =~ ^[1-9][0-9]*$ ]]; then
    error "KEZ_NJOBS must be a positive integer"
    exit 2
fi

# Application environments store an ordered sequence of package names.  The
# system environment built by init.sh stores a package-to-version map instead.
state_format=${KEZ_INSTALL_STATE_FORMAT:-sequence}
if [[ $state_format != sequence && $state_format != map ]]; then
    error "KEZ_INSTALL_STATE_FORMAT must be 'sequence' or 'map'"
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
declare -a package_command_progress=()
declare -a package_exit_status=()
declare -a package_pids=()
declare -a package_uses_python_environment=()
declare -a package_is_python_distribution=()
declare -a failed_package_indices=()
declare -a started_package_indices=()
declare -a progress_ui_dirty=()
declare -A package_indices=()
current_package_index=-1
next_finalize_position=0
running_count=0
python_environment_declared=false

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

    if [[ $package == "$PYTHON_ENVIRONMENT_PACKAGE" ]]; then
        return 0
    fi

    if [[ ${package_uses_python_environment[$index]} == true &&
        -d $target_env/.venv && -n ${KEZ_HOME:-} ]]; then
        bash "$KEZ_HOME/scripts/python_env.sh" register-package "$target_env" "$package"
    fi

    if package_is_recorded "$package"; then
        return 0
    fi

    if [[ $state_format == map ]]; then
        local version_file="$plan_runtime_dir/$index/version"
        local version
        if [[ ! -s $version_file ]]; then
            error "Package $package did not report its installed version"
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

mark_dirty() {
    progress_ui_dirty[$1]=1
}

kez_plan_begin() {
    if (( current_package_index >= 0 )); then
        error "Invalid installation plan: nested package"
        return 2
    fi

    local package=$1
    if [[ -n ${package_indices[$package]+set} ]]; then
        error "Invalid installation plan: duplicate package '$package'"
        return 2
    fi

    local index=${#plan_packages[@]}
    plan_packages+=("$package")
    package_indices["$package"]=$index
    package_status[$index]=pending
    package_command_counts[$index]=0
    package_command_progress[$index]=0
    package_exit_status[$index]=
    package_pids[$index]=
    package_uses_python_environment[$index]=false
    package_is_python_distribution[$index]=false
    mkdir -p -- "$plan_runtime_dir/$index"
    : > "$plan_runtime_dir/$index/dependencies"
    : > "$plan_runtime_dir/$index/python-requirements"
    printf '%s\n' "$package" > "$plan_runtime_dir/$index/package-name"
    printf '0\n' > "$plan_runtime_dir/$index/progress"
    current_package_index=$index
}

kez_plan_requires_python() {
    if (( current_package_index < 0 )); then
        error "Invalid installation plan: Python environment marker outside a package"
        return 2
    fi
    if [[ ${package_uses_python_environment[$current_package_index]} == false ]]; then
        package_uses_python_environment[$current_package_index]=true
        : > "$plan_runtime_dir/$current_package_index/uses-python-environment"
        printf '%s\n' "$PYTHON_ENVIRONMENT_PACKAGE" \
            >> "$plan_runtime_dir/$current_package_index/dependencies"
    fi
}

kez_plan_python_provider() {
    if (( current_package_index < 0 )) ||
        [[ ${plan_packages[$current_package_index]} != python ]]; then
        error "Invalid installation plan: Python provider marker outside the python package"
        return 2
    fi
    python_environment_declared=true
    : > "$plan_runtime_dir/$current_package_index/provides-python-environment"
}

kez_plan_python_distribution() {
    if (( current_package_index < 0 )); then
        error "Invalid installation plan: Python distribution outside a package"
        return 2
    fi
    if [[ $1 == *$'\n'* || $1 == *$'\r'* || -z $1 ]]; then
        error "Invalid installation plan: malformed Python distribution"
        return 2
    fi
    package_is_python_distribution[$current_package_index]=true
    kez_plan_requires_python
    printf '%s\n' "$1" >> "$plan_runtime_dir/$current_package_index/python-requirements"
}

kez_plan_depends() {
    if (( current_package_index < 0 )); then
        error "Invalid installation plan: dependency outside a package"
        return 2
    fi
    printf '%s\n' "$1" >> "$plan_runtime_dir/$current_package_index/dependencies"
}

kez_plan_command() {
    if (( current_package_index < 0 )); then
        error "Invalid installation plan: command outside a package"
        return 2
    fi

    local count=${package_command_counts[$current_package_index]}
    printf '%s' "$1" > "$plan_runtime_dir/$current_package_index/command-$count"
    package_command_counts[$current_package_index]=$((count + 1))
}

kez_plan_end() {
    if (( current_package_index < 0 )); then
        error "Invalid installation plan: package end without package"
        return 2
    fi
    current_package_index=-1
}

# The plan contains only shell-quoted calls to the functions above. Commands from package metadata
# are evaluated exclusively by run_package_body after dependency scheduling.
source "$plan_file"

if (( current_package_index >= 0 )); then
    error "Invalid installation plan: package was not closed"
    exit 2
fi

# Python distributions are ordinary Kez package nodes, but their virtual
# environment is shared. Add one internal scheduling node so the complete
# distribution set is installed after CPython and before those nodes and their
# native dependents.
python_environment_required=$python_environment_declared
if [[ -d $target_env/.kez-python ]]; then
    python_environment_required=true
fi
for index in "${!plan_packages[@]}"; do
    if [[ ${package_uses_python_environment[$index]} == true ]]; then
        python_environment_required=true
        break
    fi
done
if [[ $python_environment_required == true ]]; then
    kez_plan_begin "$PYTHON_ENVIRONMENT_PACKAGE"
    if [[ -n ${package_indices[python]+set} ]]; then
        kez_plan_depends python
    fi
    # Native Kez dependencies of a Python distribution must be available
    # before uv starts. Other Python distributions are part of this same
    # transaction and therefore must not become prerequisites of the barrier.
    for distribution_index in "${!plan_packages[@]}"; do
        [[ ${package_is_python_distribution[$distribution_index]} == true ]] || continue
        while IFS= read -r dependency; do
            if [[ -z $dependency || $dependency == python ||
                $dependency == "$PYTHON_ENVIRONMENT_PACKAGE" ||
                -z ${package_indices[$dependency]+set} ]]; then
                continue
            fi
            dependency_index=${package_indices[$dependency]}
            if [[ ${package_is_python_distribution[$dependency_index]} == false ]]; then
                kez_plan_depends "$dependency"
            fi
        done < "$plan_runtime_dir/$distribution_index/dependencies"
    done
    if [[ -z ${KEZ_HOME:-} ]]; then
        error "KEZ_HOME is required to prepare a Python virtual environment"
        exit 2
    fi
    printf -v python_environment_command 'bash %q sync-plan %q %q' \
        "$KEZ_HOME/scripts/python_env.sh" "$target_env" "$plan_runtime_dir"
    kez_plan_command "$python_environment_command"
    kez_plan_end
    unset python_environment_command
fi

for index in "${!plan_packages[@]}"; do
    package=${plan_packages[$index]}
    if [[ $package == "$PYTHON_ENVIRONMENT_PACKAGE" ]]; then
        continue
    fi
    if [[ $force == false ]] && package_is_recorded "$package"; then
        package_status[$index]=skipped
        package_command_progress[$index]=${package_command_counts[$index]}
        mark_dirty "$index"
    fi
done

# Interactive installations keep one terminal row per package and redraw the
# rows in place. Redirected output receives the same initial overview followed
# only by terminal state changes, so logs remain readable and free of cursor
# control sequences.
progress_ui_interactive=false
if [[ -t 1 && -t 2 && ${TERM:-dumb} != dumb ]]; then
    progress_ui_interactive=true
fi
progress_ui_rendered=false
progress_ui_package_width=0
progress_ui_step_width=1
for index in "${!plan_packages[@]}"; do
    package=${plan_packages[$index]}
    if (( ${#package} > progress_ui_package_width )); then
        progress_ui_package_width=${#package}
    fi
    command_count=${package_command_counts[$index]}
    if (( ${#command_count} > progress_ui_step_width )); then
        progress_ui_step_width=${#command_count}
    fi
done
unset command_count index package
progress_ui_spinners=('-' '\' '|' '/')
progress_ui_start_time=0
progress_bar_width=15

update_package_progress() {
    local index=$1
    local progress_file="$plan_runtime_dir/$index/progress"
    local progress
    local total=${package_command_counts[$index]}

    if [[ -s $progress_file ]] && IFS= read -r progress < "$progress_file" && \
        [[ $progress =~ ^[0-9]+$ ]]; then
        if (( progress > total )); then
            progress=$total
        fi
        package_command_progress[$index]=$progress
    fi
}

repeat_progress_character() {
    local character=$1
    local count=$2
    progress_repeated=''
    if (( count > 0 )); then
        printf -v progress_repeated '%*s' "$count" ''
        progress_repeated=${progress_repeated// /$character}
    fi
}

print_package_progress() {
    local index=$1
    local active_ordinal=${2:-0}
    local spinner_base=${3:-0}
    local package=${plan_packages[$index]}
    local status=${package_status[$index]}
    local total=${package_command_counts[$index]}
    local progress=${package_command_progress[$index]}
    local padding=$((progress_ui_package_width + 2 - ${#package}))
    local filled_width=0
    local bar
    local level=info
    local suffix=''
    local count_text
    local message

    if [[ $status == running ]]; then
        update_package_progress "$index"
        progress=${package_command_progress[$index]}
    elif [[ $status == complete || $status == done || $status == skipped ]]; then
        progress=$total
    fi

    # Normalize progress to a fixed-width bar so all packages
    # have the same visual length regardless of command count.
    if (( total > 0 )); then
        filled_width=$(( progress * progress_bar_width / total ))
    fi

    case $status in
        running)
            if (( filled_width >= progress_bar_width )); then
                # All commands done but process not yet collected.
                # No room for the spinner — show a full bar instead
                # of appending the spinner past the bar boundary.
                repeat_progress_character '#' "$progress_bar_width"
                bar=$progress_repeated
            else
                repeat_progress_character '#' "$filled_width"
                local filled=$progress_repeated
                local unfilled_width=$(( progress_bar_width - filled_width - 1 ))
                repeat_progress_character '.' "$unfilled_width"
                bar="${filled}${progress_ui_spinners[$(((spinner_base + active_ordinal) % 4))]}${progress_repeated}"
            fi
            ;;
        complete | done)
            repeat_progress_character '#' "$progress_bar_width"
            bar=$progress_repeated
            level=success
            ;;
        skipped)
            repeat_progress_character '#' "$progress_bar_width"
            bar=$progress_repeated
            level=warning
            suffix=' (already installed)'
            ;;
        failed)
            if (( filled_width >= progress_bar_width - 1 )); then
                repeat_progress_character '#' "$((progress_bar_width - 1))"
                bar="${progress_repeated}!"
            else
                repeat_progress_character '#' "$filled_width"
                local filled=$progress_repeated
                local unfilled_width=$(( progress_bar_width - filled_width - 1 ))
                repeat_progress_character '.' "$unfilled_width"
                bar="${filled}!${progress_repeated}"
            fi
            level=error
            suffix=' (failed)'
            ;;
        *)
            repeat_progress_character '.' "$progress_bar_width"
            bar=$progress_repeated
            progress=0
            ;;
    esac

    printf -v count_text '%*d/%*d' "$progress_ui_step_width" "$progress" \
        "$progress_ui_step_width" "$total"
    printf -v message '%s:%*scommand %s [%s]%s' "$package" "$padding" '' "$count_text" "$bar" \
        "$suffix"
    colored_print "$level" "$message"
}

render_progress_ui() {
    local index
    local active_ordinal=0
    local line_count=${#plan_packages[@]}
    local spinner_base=0

    # Compute the time-based spinner frame once per render so all
    # running packages share the same base, giving them consistent
    # animation speed regardless of loop iteration duration.
    if (( progress_ui_start_time > 0 )); then
        local elapsed_ms=$(( ( $(date +%s%N) - progress_ui_start_time ) / 1000000 ))
        spinner_base=$(( elapsed_ms / SPINNER_INTERVAL_MS ))
    fi

    # Non-interactive: full dump once, then incremental per-completion only
    if [[ $progress_ui_interactive == false ]]; then
        if [[ $progress_ui_rendered == false ]]; then
            for index in "${!plan_packages[@]}"; do
                print_package_progress "$index" "$active_ordinal" "$spinner_base"
                if [[ ${package_status[$index]} == running ]]; then
                    active_ordinal=$((active_ordinal + 1))
                fi
            done
            progress_ui_rendered=true
        fi
        return 0
    fi

    # Interactive: move cursor to the top of the package list
    if [[ $progress_ui_rendered == true && line_count -gt 0 ]]; then
        printf '\033[%dA' "$line_count"
    fi

    for index in "${!plan_packages[@]}"; do
        if [[ ${package_status[$index]} == running || \
              ${progress_ui_dirty[$index]-} == 1 ]]; then
            # Carriage return + overwrite in-place. No \033[2K clear — the
            # static prefix up to "command " never changes, so rewriting it
            # without blanking first avoids visible flicker. Only the count
            # and progress bar differ between frames.
            printf '\r'
            print_package_progress "$index" "$active_ordinal" "$spinner_base"
            progress_ui_dirty[$index]=0
        elif [[ $progress_ui_rendered == true ]]; then
            # Unchanged line: advance cursor, leave old content visible
            printf '\n'
        else
            # First render: draw everything
            print_package_progress "$index" "$active_ordinal" "$spinner_base"
            progress_ui_dirty[$index]=0
        fi
        if [[ ${package_status[$index]} == running ]]; then
            active_ordinal=$((active_ordinal + 1))
        fi
    done
    progress_ui_rendered=true
}

info "Logs are available at $target_env/logs"
render_progress_ui

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
    if [[ ${package_uses_python_environment[$index]} == true ]]; then
        if [[ ! -x $target_env/.venv/bin/python ]]; then
            error "Python virtual environment is missing for package $package"
            return 1
        fi
        export VIRTUAL_ENV="$target_env/.venv"
        export PATH="$VIRTUAL_ENV/bin:$PATH"
        export PYTHONNOUSERSITE=1
        unset PYTHONHOME PYTHONPATH
    fi
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
        printf '%s\n' "$((command_index + 1))" > "$plan_runtime_dir/$index/progress"
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

    package_status[$index]=running
    mark_dirty "$index"
    rm -f -- "$plan_runtime_dir/$index/exit-status"
    (
        trap - EXIT
        set +e
        run_package_body "$index"
        status=$?
        printf '%s\n' "$status" > "$plan_runtime_dir/$index/exit-status"
        exit "$status"
    ) &
    package_pids[$index]=$!
    started_package_indices+=("$index")
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

finalize_completed_packages() {
    local index
    local package
    local completed_status

    # State updates remain deterministic even when sibling builds finish in
    # different orders. A completed process no longer consumes a job slot or
    # shows a spinner while it waits for earlier-dispatched work to finish.
    while (( next_finalize_position < ${#started_package_indices[@]} )); do
        index=${started_package_indices[$next_finalize_position]}
        if [[ ${package_status[$index]} == running ]]; then
            break
        fi

        package=${plan_packages[$index]}
        completed_status=${package_exit_status[$index]}
        if [[ ${package_status[$index]} == complete ]]; then
            if mark_package_installed "$package" "$index"; then
                package_status[$index]=done
                mark_dirty "$index"
                cleanup_package_work_dir "$package"
            else
                completed_status=$?
                package_exit_status[$index]=$completed_status
                package_status[$index]=failed
                mark_dirty "$index"
            fi
        fi

        if [[ ${package_status[$index]} == failed ]]; then
            failed_package_indices+=("$index")
            if (( failure_status == 0 )); then
                failure_status=$completed_status
            fi
        fi
        next_finalize_position=$((next_finalize_position + 1))
    done
}

collect_completed_packages() {
    local index
    local pid
    local completed_status
    local running_pids
    local -a completed_indices=()

    # The exit-status file is the fast path. Checking Bash's job table as
    # well prevents a command that calls `exit` or receives a signal from
    # leaving the scheduler waiting forever before it can write that file.
    running_pids=$'\n'"$(jobs -pr)"$'\n'

    for index in "${!plan_packages[@]}"; do
        if [[ ${package_status[$index]} != running ]]; then
            continue
        fi

        pid=${package_pids[$index]}
        if [[ ! -s $plan_runtime_dir/$index/exit-status && \
            $running_pids == *$'\n'"$pid"$'\n'* ]]; then
            continue
        fi
        if wait "$pid" 2>/dev/null; then
            completed_status=0
        else
            completed_status=$?
        fi
        package_pids[$index]=
        running_count=$((running_count - 1))
        update_package_progress "$index"
        package_exit_status[$index]=$completed_status
        completed_indices+=("$index")

        if (( completed_status == 0 )); then
            package_status[$index]=complete
            package_command_progress[$index]=${package_command_counts[$index]}
        else
            package_status[$index]=failed
            if (( failure_status == 0 )); then
                failure_status=$completed_status
            fi
        fi
        mark_dirty "$index"
    done

    finalize_completed_packages

    for index in "${completed_indices[@]}"; do
        if [[ $progress_ui_interactive == false ]]; then
            print_package_progress "$index"
        fi
    done
}

progress_ui_start_time=$(date +%s%N)

while true; do
    started_package=false
    # Start as many ready packages as the job limit and failure status allow.
    while (( failure_status == 0 && running_count < KEZ_NJOBS )); do
        find_next_ready_package
        if (( next_ready_index < 0 )); then
            break
        fi
        start_package "$next_ready_index"
        started_package=true
    done

    # If nothing is running, all pending packages have unmet dependencies
    # or we're out of work — exit the scheduling loop.
    if (( running_count == 0 )); then
        break
    fi

    # Poll every package instead of blocking on one PID. This keeps all active
    # indicators moving and lets newly-unblocked work start promptly.
    sleep "$ANIMATION_INTERVAL"
    collect_completed_packages
    if [[ $progress_ui_interactive == true ]]; then
        render_progress_ui
    fi
done

for index in "${failed_package_indices[@]}"; do
    package=${plan_packages[$index]}
    log_file=$(package_log_file "$package")
    error "Package $package failed. Read log file: $log_file"
done

if (( failure_status != 0 )); then
    exit "$failure_status"
fi

if has_pending_packages; then
    error "Invalid installation plan: dependency cycle or missing dependency prevented progress"
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
    modulefile_tmp="$modulefile.tmp.$$"
    "${KEZ_HOME}/scripts/gen_modulefile.sh" "$target_env" > "$modulefile_tmp"
    mv -f -- "$modulefile_tmp" "$modulefile"
    success "Updated module file: $modulefile"
fi
