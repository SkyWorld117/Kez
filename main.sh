#!/usr/bin/env bash

# Fromager wrapper script that forwards arguments to the `fromager` command except:

# Fromager location: ${FROMAGER_HOME}/bin/fromager

# - `cellar enter <cellar>`
# - `cellar exit`
# - `compiler load <compiler>`
# - `compiler unload`
# - `mpi load <mpi>`
# - `mpi unload`
# - `rt factory enter <factory>`
# - `rt factory exit`

# These commands will return bash instructions that will be evaluated by this wrapper script to change the current shell environment.

fgr () {
    if [ ! -f "${FROMAGER_HOME}/bin/fromager" ] && [ "${1:-}" == "init" ]; then
        echo "[W]: Fromager is not initialized. Running initialization process..."
        ${FROMAGER_HOME}/bin/fromager_init
        return
    fi

    local intercept=0
    
    if [[ "${1:-}" == "cellar" && ( "${2:-}" == "enter" || "${2:-}" == "exit" ) ]]; then
        intercept=1
    elif [[ "${1:-}" == "compiler" && ( "${2:-}" == "load" || "${2:-}" == "unload" ) ]]; then
        intercept=1
    elif [[ "${1:-}" == "mpi" && ( "${2:-}" == "load" || "${2:-}" == "unload" ) ]]; then
        intercept=1
    elif [[ "${1:-}" == "rt" && "${2:-}" == "factory" && ( "${3:-}" == "enter" || "${3:-}" == "exit" ) ]]; then
        intercept=1
    fi

    if [[ $intercept -eq 1 ]]; then
        eval "$("${FROMAGER_HOME}/bin/fromager" "$@")"
    else
        "${FROMAGER_HOME}/bin/fromager" "$@"
    fi
}

export -f fgr