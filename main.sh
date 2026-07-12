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
    else
        "${KEZ_HOME}/bin/kez" "$@"
    fi
}

export -f kez
