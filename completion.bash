#!/usr/bin/env bash

_kez_complete() {
    if [ -f "${KEZ_HOME}/bin/bash_completion" ]; then
        # Call the completion script and capture its output
        COMPREPLY=($("${KEZ_HOME}/bin/bash_completion" "$COMP_CWORD" "${COMP_WORDS[@]}"))
    else
        if [[ $COMP_CWORD == 1 ]]; then
            COMPREPLY=($(compgen -W "init" -- "${COMP_WORDS[COMP_CWORD]}"))
        elif [[ ${COMP_WORDS[1]} == "init" ]]; then
            COMPREPLY=($(compgen -W "--refresh --use-distro-compiler" -- "${COMP_WORDS[COMP_CWORD]}"))
        fi
    fi
}

# Register the completion function
complete -o filenames -F _kez_complete kez
