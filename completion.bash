#!/usr/bin/env bash

_kez_complete() {
    local completion_binary="${KEZ_HOME}/bin/kez_completion"
    if [[ -x ${completion_binary} ]]; then
        mapfile -t COMPREPLY < <("${completion_binary}" "${COMP_CWORD}" "${COMP_WORDS[@]}")
    elif [[ ${COMP_CWORD} == 1 ]]; then
        mapfile -t COMPREPLY < <(compgen -W "init" -- "${COMP_WORDS[COMP_CWORD]}")
    elif [[ ${COMP_WORDS[1]} == "init" ]]; then
        mapfile -t COMPREPLY < <(
            compgen -W "--force --use-distro-compiler" -- "${COMP_WORDS[COMP_CWORD]}"
        )
    fi
}

# Register the completion function
complete -o filenames -F _kez_complete kez
