#!/bin/bash

# Bash completion for fgr (Fromager)
_fgr_complete() {
    if [ -f "${FROMAGER_HOME}/bin/fromager_bash_completion" ]; then
        # Call the completion script and capture its output
        COMPREPLY=($("${FROMAGER_HOME}/bin/fromager_bash_completion" "$COMP_CWORD" "${COMP_WORDS[@]}"))
    else
        if [[ ${COMP_CWORD} == 1 ]]; then
            COMPREPLY=($(compgen -W "init" -- "${COMP_WORDS[COMP_CWORD]}"))
        elif [[ ${COMP_WORDS[1]} == "init" ]]; then
            COMPREPLY=($(compgen -W "--with-slurm --refresh" -- "${COMP_WORDS[COMP_CWORD]}"))
        fi
    fi
}

# Register the completion function
complete -o filenames -F _fgr_complete fgr