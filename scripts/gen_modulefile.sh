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

# We iterate over all subdirectories in the environment.
# We exclude hidden directories (like .tmp).
for pkg_dir in $(find "$target_env" -maxdepth 1 -mindepth 1 -type d ! -name ".*"); do
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
done
