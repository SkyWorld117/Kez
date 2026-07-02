#!/usr/bin/env bash

source /dev/stdin <<EOF
_${ABBREV}_complete() {
    if [ -f "${!_HOME}/bin/bash_completion" ]; then
        # Call the completion script and capture its output
        COMPREPLY=(\$("${!_HOME}/bin/bash_completion" "\$COMP_CWORD" "\${COMP_WORDS[@]}"))
    else
        if [[ \$COMP_CWORD == 1 ]]; then
            COMPREPLY=(\$(compgen -W "init" -- "\${COMP_WORDS[COMP_CWORD]}"))
        elif [[ \${COMP_WORDS[1]} == "init" ]]; then
            COMPREPLY=(\$(compgen -W "--refresh --use-distro-compiler" -- "\${COMP_WORDS[COMP_CWORD]}"))
        fi
    fi
}
EOF

# Register the completion function
complete -o filenames -F _${ABBREV}_complete ${ABBREV}
