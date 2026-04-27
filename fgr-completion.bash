#!/bin/bash

# Bash completion for fgr (Fromager)
_fgr_complete() {
    if [ -f "${FROMAGER_HOME}/bin/fromager_bash_completion" ]; then
        # Call the completion script and capture its output
        COMPREPLY=($("${FROMAGER_HOME}/bin/fromager_bash_completion" "$COMP_CWORD" "${COMP_WORDS[@]}"))
    else
        COMPREPLY=()
    fi
}

# Register the completion function
complete -F _fgr_complete fgr