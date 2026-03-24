#!/bin/bash

# Bash completion for fgr (Fromager)
_fgr_complete() {
    COMPREPLY=($(fromager_bash_completion "$COMP_CWORD" "${COMP_WORDS[@]}"))
}

# Register the completion function
complete -o filenames -F _fgr_complete fgr