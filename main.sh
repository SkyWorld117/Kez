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

# These commands will return bash instructions that will be evaluated by this wrapper script to change the current shell environment.

source /dev/stdin <<EOF
${ABBREV} () {
    if [ ! -f "${!_HOME}/bin/${BINARY}" ]; then
        if [ "\${1:-}" == "init" ]; then
            echo "[W]: ${PROJECT} is not initialized. Running initialization process..."

            local cmd="bash ${!_HOME}/scripts/init.sh"

            if [ "\${2:-}" == "--refresh" ] || [ "\${3:-}" == "--refresh" ]; then
                cmd="\${cmd} --refresh"
            fi

            if [ "\${2:-}" == "--use-distro-compiler" ] || [ "\${3:-}" == "--use-distro-compiler" ]; then
                cmd="\${cmd} --use-distro-compiler"
            fi

            eval "\${cmd}"

            return \$?
        fi

        echo >&2 "[E]: ${PROJECT} is not initialized. Please run '${ABBREV} init' to initialize ${PROJECT} before using other commands."
        return 1
    fi

    local intercept=0
    
    if [[ "\${1:-}" == "env" && ( "\${2:-}" == "enter" || "\${2:-}" == "exit" ) ]]; then
        intercept=1
    elif [[ "\${1:-}" == "compiler" && ( "\${2:-}" == "load" || "\${2:-}" == "unload" ) ]]; then
        intercept=1
    elif [[ "\${1:-}" == "mpi" && ( "\${2:-}" == "load" || "\${2:-}" == "unload" ) ]]; then
        intercept=1
    elif [[ "\${1:-}" == "factory" && ( "\${2:-}" == "enter" || "\${2:-}" == "exit" ) ]]; then
        intercept=1
    fi

    if [[ \$intercept -eq 1 ]]; then
        eval "\$("${!_HOME}/bin/${BINARY}" "$@")"
    else
        "${!_HOME}/bin/${BINARY}" "$@"
    fi
}
EOF

export -f ${ABBREV}
