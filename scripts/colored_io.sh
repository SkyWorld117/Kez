#!/usr/bin/env bash

# Lightweight colored status output for shell code that runs before the Kez
# binaries are available. Keep these colors and prefixes in sync with
# include/utils/colors.h and include/utils/colored_io.hpp.

colored_print() {
    local level=$1
    shift
    local message=$*
    local prefix
    local color_code
    local output_fd=1

    case $level in
        info)
            prefix=I
            color_code=34
            ;;
        warning)
            prefix=W
            color_code=33
            ;;
        error)
            prefix=E
            color_code=31
            output_fd=2
            ;;
        success)
            prefix=S
            color_code=32
            ;;
        *)
            printf '[E]: Unknown message type: %s\n' "$level" >&2
            return 2
            ;;
    esac

    if [[ -t $output_fd && ${TERM:-dumb} != dumb ]]; then
        printf '\033[%sm[%s]: %s\033[0m\n' "$color_code" "$prefix" "$message" >&"$output_fd"
    else
        printf '[%s]: %s\n' "$prefix" "$message" >&"$output_fd"
    fi
}

info() { colored_print info "$@"; }

warning() { colored_print warning "$@"; }

# Warnings emitted from value-producing command substitutions must stay off
# stdout. This short alias is used by the bootstrap download helpers.
warn() { warning "$@" >&2; }

error() { colored_print error "$@"; }

success() { colored_print success "$@"; }
