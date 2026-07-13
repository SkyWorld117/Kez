#!/usr/bin/env bash

# Wrapper script that forwards arguments to the binary except:

# - `env enter <env>`
# - `env exit`
# - `compiler load <compiler>`
# - `compiler unload`
# - `mpi load <mpi>`
# - `mpi unload`
# - `factory enter <factory>`
# - `factory exit`
# These commands return bash instructions that this wrapper evaluates in the current shell.
# - `utilities add`     (run install, then update PATH directly in the shell)
# - `utilities remove`  (run remove, then remove bin/ from PATH in the shell)

kez () {
    ################################################
    # This is a wrapper function of Kez            #
    # The binary is located in ${KEZ_HOME}/bin/kez #
    ################################################

    if [ ! -f "${KEZ_HOME}/bin/kez" ]; then
        if [ "${1:-}" == "init" ]; then
            echo "[W]: Kez is not initialized. Running initialization process..."

            local cmd="bash ${KEZ_HOME}/scripts/init.sh"

            if [ "${2:-}" == "--refresh" ] || [ "${3:-}" == "--refresh" ]; then
                cmd="${cmd} --refresh"
            fi

            if [ "${2:-}" == "--use-distro-compiler" ] || [ "${3:-}" == "--use-distro-compiler" ]; then
                cmd="${cmd} --use-distro-compiler"
            fi

            eval "${cmd}"

            return $?
        fi

        echo >&2 "[E]: Kez is not initialized. Please run 'kez init' to initialize Kez before using other commands."
        return 1
    fi

    local intercept=0

    if [[ ( "${1:-}" == "env" || "${1:-}" == "cellar" ) && ( "${2:-}" == "enter" || "${2:-}" == "exit" ) ]]; then
        intercept=1
    elif [[ "${1:-}" == "compiler" && ( "${2:-}" == "load" || "${2:-}" == "unload" ) ]]; then
        intercept=1
    elif [[ "${1:-}" == "mpi" && ( "${2:-}" == "load" || "${2:-}" == "unload" ) ]]; then
        intercept=1
    elif [[ "${1:-}" == "factory" && ( "${2:-}" == "enter" || "${2:-}" == "exit" ) ]]; then
        intercept=1
    fi

    if [[ $intercept -eq 1 ]]; then
        local shell_commands ret=0
        shell_commands="$("${KEZ_HOME}/bin/kez" "$@")" || ret=$?
        if (( ret )); then
            return $ret
        fi
        eval "${shell_commands}"
    elif [[ "${1:-}" == "utilities" && "${2:-}" == "add" ]]; then
        # Run install (output goes to terminal), then update PATH so
        # newly installed utilities are immediately available.
        "${KEZ_HOME}/bin/kez" "$@"
        local ret=$?
        if (( ret )); then
            return $ret
        fi
        local _kez_util_root _kez_util_dir
        _kez_util_root="${KEZ_WORKDIR}/$(yq -r '.paths.utilities' "${KEZ_HOME}/manifest.yaml")"
        for _kez_util_dir in "${_kez_util_root}"/*; do
            [[ -d "${_kez_util_dir}/bin" ]] || continue
            if [[ ":$PATH:" != *":${_kez_util_dir}/bin:"* ]]; then
                export PATH="${_kez_util_dir}/bin:${PATH}"
            fi
        done
        unset _kez_util_root _kez_util_dir
    elif [[ "${1:-}" == "utilities" && "${2:-}" == "remove" ]]; then
        local _kez_pkg="${3:-}"
        if [[ -z $_kez_pkg ]]; then
            "${KEZ_HOME}/bin/kez" "$@"
            return $?
        fi
        local _kez_rm_dir
        _kez_rm_dir="${KEZ_WORKDIR}/$(yq -r '.paths.utilities' "${KEZ_HOME}/manifest.yaml")/${_kez_pkg}/bin"
        "${KEZ_HOME}/bin/kez" "$@"
        local ret=$?
        if (( ret )); then
            return $ret
        fi
        local _kez_nw='' _kez_ifs _kez_p
        _kez_ifs="$IFS"; IFS=:
        for _kez_p in $PATH; do
            if [[ $_kez_p != "$_kez_rm_dir" ]]; then
                _kez_nw="${_kez_nw:+$_kez_nw:}$_kez_p"
            fi
        done
        IFS="$_kez_ifs"
        export PATH="$_kez_nw"
        unset _kez_nw _kez_ifs _kez_p _kez_rm_dir _kez_pkg
    else
        "${KEZ_HOME}/bin/kez" "$@"
    fi
}

export -f kez
