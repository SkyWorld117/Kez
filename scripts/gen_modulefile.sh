#!/usr/bin/env bash
set -Eeuo pipefail

# Helper script to generate a Tcl modulefile for a Kez environment.
# It iterates over isolated package directories and adds their bin,
# man pages, and pkg-config paths to the environment.

if [[ $# -lt 1 ]]; then
    printf "Usage: %s <environment_path>\n" "$0" >&2
    exit 1
fi

target_env=$(realpath "$1")
env_name=$(basename "$target_env")

printf '#%%Module1.0\n'
printf 'module-whatis "Loads the %s Kez environment"\n' "$env_name"

# We iterate over all package subdirectories in deterministic order. Hidden
# executor directories and the explicitly handled .venv are excluded.
while IFS= read -r -d '' pkg_dir; do
    # Add bin directory to PATH
    bin_dir="$pkg_dir/bin"
    if [[ -d "$bin_dir" ]]; then
        printf 'prepend-path PATH "%s"\n' "$bin_dir"
    fi

    # Add man pages to MANPATH
    man_dir="$pkg_dir/share/man"
    if [[ -d "$man_dir" ]]; then
        printf 'prepend-path MANPATH "%s"\n' "$man_dir"
    fi

    # Add pkg-config directories to PKG_CONFIG_PATH
    # Common paths for pkg-config are lib/pkgconfig, lib64/pkgconfig, and share/pkgconfig
    for pkgcfg_dir in "$pkg_dir/lib/pkgconfig" "$pkg_dir/lib64/pkgconfig" "$pkg_dir/share/pkgconfig"; do
        if [[ -d "$pkgcfg_dir" ]]; then
            printf 'prepend-path PKG_CONFIG_PATH "%s"\n' "$pkgcfg_dir"
        fi
    done
done < <(find "$target_env" -maxdepth 1 -mindepth 1 -type d ! -name ".*" -print0 | sort -z)

# Emit the venv last: modulecmd applies each prepend-path in sequence, so this
# leaves the venv interpreter ahead of package-provided executables.
if [[ -d $target_env/.venv/bin ]]; then
    printf 'setenv VIRTUAL_ENV "%s"\n' "$target_env/.venv"
    printf 'prepend-path PATH "%s"\n' "$target_env/.venv/bin"
fi
